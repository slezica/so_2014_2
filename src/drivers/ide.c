// Driver de discos IDE

// Soporta solamente discos ATA, dos controladores (primario y secundario)
// con dos discos cada uno (master y slave), con direcciones de entrada/salida
// standard e interrupciones standard. Utiliza modo PIO por interrupción y
// requiere que los discos soporten LBA (no maneja el protocolo CHS) y
// entrada/salida de múltiples sectores por interrupción (no usa el modo 
// más ineficiente de transferir un solo sector por interrupción).

#include <kernel.h>

// Dimensionamiento
#define NR_CONTROLLERS 					2
#define DEVS_PER_CONTROLLER				2
#define PRIMARY  						0
#define SECONDARY						1
#define MASTER      					0
#define SLAVE     						1

// Direcciones de entrada/salida
#define PRIMARY_IOBASE  				0x1F0
#define SECONDARY_IOBASE				0x170

// Interrupciones
#define PRIMARY_IRQ						14
#define SECONDARY_IRQ					15

// Offsets de los registros
#define DATA        					0
#define ERROR       					1
#define NSECTOR     					2
#define SECTOR      					3
#define LCYL        					4
#define HCYL        					5
#define DRV_HEAD    					6
#define STATUS      					7
#define COMMAND     					7
#define ALT_STATUS    					0x206
#define DEV_CTL     					0x206

// Comandos
#define ATA_IDENTIFY    				0xEC
#define ATAPI_IDENTIFY  				0xA1
#define ATA_READ_BLOCK  				0x20
#define ATA_WRITE_BLOCK 				0x30
#define ATA_READ_MULTI  				0xC4
#define ATA_WRITE_MULTI 				0xC5
#define ATA_SET_MULTI					0xC6

// Bits del registro de status
#define STATUS_BSY  					0x80
#define STATUS_DRDY 					0x40
#define STATUS_DRQ  					0x08
#define STATUS_ERR  					0x01

// Bits del registro de control
#define CTL_SRST    					0x04
#define CTL_nIEN    					0x02

// Timeout para los comandos (ms)
#define TIMEOUT 						31000

// Operaciones
#define READ     						0
#define WRITE    						1

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned long uint32_t;

typedef struct ide_device ide_device;
typedef struct ide_controller ide_controller;

struct ide_device
{
	ide_controller *controller;					// Controlador de este dispositivo
	unsigned position;							// Maestro/esclavo
	bool present;								// Dispositivo existente e inicializado

	char model[40];								// Strings de identificación
	char serial[20];
	char firmware[8];

	bool atapi;									// Es un dispositivo de paquetes
	bool dma;									// Soporta DMA
	unsigned capacity;							// Capacidad en sectores
	unsigned max_multi;							// Máxima cantidad de sectores para read/write multiple
};

struct ide_controller
{
	unsigned iobase;							// Dirección base de entrada/salida
	unsigned irq;								// Interrupción
	ide_device devices[DEVS_PER_CONTROLLER];	// Dispositivos de este controlador
	Mutex_t *mutex;								// Exclusión mutua
	Semaphore_t *sem;							// Semáforo para la interrupción
};

// Los controladores con sus dispositivos
static ide_controller controllers[NR_CONTROLLERS] =
{
	{
		.iobase = PRIMARY_IOBASE,
		.irq = PRIMARY_IRQ,
		.devices = 
	 	{
			{ .position = MASTER },
			{ .position = SLAVE  }
		}
	},
	{
		.iobase = SECONDARY_IOBASE,
		.irq = SECONDARY_IRQ,
		.devices = 
		{
			 { .position = MASTER },
			 { .position = SLAVE  }
		}
	}
};

// Cambiar el orden de los bytes en los strings de identificación
static void
fix(char *s, int len)
{
	char c, *p = s, *end = s + (len & ~1);

	while ( p != end )
	{
		c = *p;
		*p = *(p + 1);
		*(p + 1) = c;
		p += 2;
	}

	p = end - 1;
	*p-- = 0;

	while ( p-- != s )
	{
		c = *p;
		if ( c > 32 && c < 127 )
			break;
		*p = 0;
	}
}

// Esperar BSY==0, leyendo el status alternativo.
// Después de un breve período inicial empieza a soltar la CPU entre encuestas sucesivas.
// Fracasa por timeout esperando BSY==0 o si, cuando BSY==0, el enmascaramiento del status 
// con la máscara es distinto del valor.
static bool
wait_notbusy(ide_controller *controller, uint8_t mask, uint8_t value)
{
	Time_t t;
	Time_t deadline = 0, start;
	unsigned status;

	UDelay(1);

	while ( (status = inb(controller->iobase + ALT_STATUS)) & STATUS_BSY )
		if ( deadline )
		{
			t = Time();
			if ( t > deadline )
				return false;
			if ( t == start )
				Yield();
			else
				Delay(10);
		}
		else
		{
			start = Time();
			deadline = start + TIMEOUT;
			Yield();
		}

	return (status & mask) == value;
}

// Verificar que un controlador exista
static bool
is_controller(ide_controller *controller)
{
	return inb(controller->iobase + STATUS) != 0xFF;
}

// Resetear un controlador
static bool
reset_controller(ide_controller *controller)
{
	int iobase = controller->iobase;

	UDelay(5);
	outb(iobase + DEV_CTL, CTL_SRST);
	UDelay(5);
	outb(iobase + DEV_CTL, 0);
	UDelay(2000);

	return wait_notbusy(controller, 0, 0);
}

// Seleccionar un dispositivo
static bool
select_device(ide_device *device)
{
	ide_controller *controller = device->controller;
	int iobase = controller->iobase;

	if ( !wait_notbusy(controller, STATUS_DRQ, 0) )
		return false;

	outb(iobase + DRV_HEAD, 0xa0 | (device->position << 4));

	return wait_notbusy(controller, STATUS_DRQ, 0);
}

// Identificar un dispositivo
static void
identify_device(ide_device *device)
{
	ide_controller *controller = device->controller;
	int i, iobase = controller->iobase;
	uint8_t status, cl, ch, cmd;
	uint16_t info[SECTOR_SIZE/2];

	device->present = false;

	if ( !is_controller(controller) || !reset_controller(controller) || !select_device(device) )
		return;

	// Leer la firma del dispositivo
	if ( inb(iobase + NSECTOR) == 0x01 && inb(iobase + SECTOR) == 0x01 )
	{
		cl = inb(iobase + LCYL);
		ch = inb(iobase + HCYL);
		status = inb(iobase + STATUS);
		if ( cl == 0x14 && ch == 0xeb )
		{
			// ATAPI (interfaz de paquetes)
			device->present = true;
			device->atapi = true;

		}
		else if ( cl == 0 && ch == 0 && status != 0 )
		{
			// ATA (disco común)
			device->present = true;
			device->atapi = false;
		}
	}

	if ( !device->present )
		return;

	// Comando de identificación
	cmd = device->atapi ? ATAPI_IDENTIFY : ATA_IDENTIFY;
	outb(iobase + COMMAND, cmd);
	if ( !wait_notbusy(controller, STATUS_ERR, 0) )
	{
		device->present = false;
		return;
	}

	// Leer la info de identificación
	for ( i = 0 ; i < SECTOR_SIZE/2 ; i++ )
		info[i] = inw(iobase + DATA);

	if ( !((info[49] >> 9) & 1) )		// No soporta LBA
	{
		device->present = false;		// Lástima, sólo usamos LBA
		return;
	}

	device->dma = (info[49] >> 8) & 1;
	
	memcpy(device->model, &info[27], 40);
	fix(device->model, 40);
	memcpy(device->serial, &info[10], 20);
	fix(device->serial, 20);
	memcpy(device->firmware, &info[23], 8);
	fix(device->firmware, 8);

	if ( device->atapi )
		return;

	device->capacity = *(unsigned *)&info[60];

	// Verificar que soporta read/write multiple
	if ( !(device->max_multi = info[47] & 0xFF) )
		device->present = false;
}

// Verifica un número de disco
static inline bool
check(unsigned minor)
{
	return minor < NR_CONTROLLERS * DEVS_PER_CONTROLLER;
}

// Devuelve el dispositivo correspondiente a un número de disco.
static ide_device *
get_device(unsigned minor)
{
	ide_controller *controller;
	ide_device *device;

	controller = &controllers[minor / DEVS_PER_CONTROLLER];
	device = &controller->devices[minor % DEVS_PER_CONTROLLER];
	return device;
}

// Función de lectura/escritura
static unsigned
read_write_blocks(unsigned minor, unsigned block, unsigned nblocks, void *buffer, int type)
{
	uint16_t *buf;
	int i;
	Time_t deadline;

	if ( !check(minor) )
		return 0;

	ide_device *device = get_device(minor);
	ide_controller *controller = device->controller;
	unsigned iobase = controller->iobase;
	Mutex_t *mutex = controller->mutex;
	Semaphore_t *sem = controller->sem;

	if ( !device->present || device->atapi || !nblocks || block >= device->capacity )
		return 0;

	if ( block + nblocks > device->capacity )
		nblocks = device->capacity - block;

	if ( nblocks > device->max_multi )
		nblocks = device->max_multi;

	// Serializar acceso al controlador
	EnterMutex(mutex);

	// Seleccionar dispositivo
	if ( !select_device(device) )
	{
		LeaveMutex(mutex);
		return 0;
	}

	// Setear cantidad de sectores para read/write multiple
	outb(iobase + NSECTOR, device->max_multi);
	outb(iobase + COMMAND, ATA_SET_MULTI);
	if ( !wait_notbusy(controller, STATUS_ERR, 0) )
	{
		LeaveMutex(mutex);
		return 0;
	}

	// Limpiar el semáforo de interrupciones anteriores
	FlushSem(sem, false);

	// Ejecutar comando de lectura o escritura
	outb(iobase + NSECTOR, nblocks);
	outb(iobase + SECTOR, block);
	outb(iobase + LCYL, block >> 8);
	outb(iobase + HCYL, block >> 16);
	outb(iobase + DRV_HEAD, (1 << 6) | (device->position << 4) | ((block >> 24) & 0x0F));
	outb(iobase + COMMAND, type == READ ? ATA_READ_MULTI : ATA_WRITE_MULTI);

	// Si es una escritura, escribir los datos ahora 
	if ( type == WRITE )
	{
		if ( !wait_notbusy(controller, STATUS_ERR, 0) )
		{
			LeaveMutex(mutex);
			return 0;
		}
		for ( buf = buffer, i = nblocks * (SECTOR_SIZE/2) ; i-- ; )
			outw(iobase + DATA, *buf++);
	}

	// Esperar interrupción con timeout, verificando error cada 100 ms
	deadline = Time() + TIMEOUT;
	while ( !WaitSemTimed(sem, 100) )
	{
		unsigned status = inb(iobase + ALT_STATUS);
		if ( (status & (STATUS_ERR | STATUS_BSY)) == STATUS_ERR || Time() > deadline )
		{
			LeaveMutex(mutex);
			return 0;
		}
	}

	// Si es una lectura, leer los datos ahora 
	if ( type == READ )
		for ( buf = buffer, i = nblocks * (SECTOR_SIZE/2) ; i-- ; )
			*buf++ = inw(iobase + DATA);

	LeaveMutex(mutex);

	return nblocks;
}

// Manejador de interrupción
static void
ide_interrupt(unsigned irq)
{
	ide_controller *controller;
	unsigned i;

	for ( i = 0 , controller = controllers ; i < NR_CONTROLLERS ; i++, controller++ )
		if ( controller->irq == irq )
		{
			inb(controller->iobase + STATUS);
			SignalSem(controller->sem);
			break;
		}
}

// Interfaz pública

// Inicialización
void
mt_ide_init(void)
{
	int i, j;
	ide_device *device;
	ide_controller *controller;
	char buf[100];

	for ( i = 0, controller = controllers ; i < NR_CONTROLLERS ; i++, controller++ )
	{
		// Inicializar la estructura del controlador
		sprintf(buf, "IDE %u mutex", i);
		controller->mutex = CreateMutex(buf);
		sprintf(buf, "IDE %u semaphore", i);
		controller->sem = CreateSem(buf, 0);

		// Identificar los discos conectados a este controlador
		for ( j = 0, device = controller->devices ; j < DEVS_PER_CONTROLLER ; j++, device++ )
		{
			// Inicializar la estructura del dispositivo
			device->controller = controller;
			identify_device(device);
#if 0
			if ( !device->present )
				continue;
			if ( device->atapi )
				printk("ATAPI[%u,%u]:%s DMA:%s\n", 
					i, j, device->model, device->dma ? "SI" : "NO");
			else
				printk("ATA[%u,%u]:%s CAPACIDAD:%u sectores MULTI:%d DMA:%s\n", 
					i, j, device->model, device->capacity, device->max_multi, device->dma ? "SI" : "NO");
#endif
		}

		// Instalar y habilitar interrupción
		mt_set_int_handler(controller->irq, ide_interrupt);
		mt_enable_irq(controller->irq);
	}
}

// Leer sectores. Devuelve la cantidad leída, que puede ser menor que la solicitada.
unsigned
mt_ide_read(unsigned minor, unsigned block, unsigned nblocks, void *buffer)
{
	return read_write_blocks(minor, block, nblocks, buffer, READ);
}

// Escribir sectores. Devuelve la cantidad escrita, que puede ser menor que la solicitada.
unsigned
mt_ide_write(unsigned minor, unsigned block, unsigned nblocks, void *buffer)
{
	return read_write_blocks(minor, block, nblocks, buffer, WRITE);
}

// Devuelve el modelo del disco, o NULL si no está presente
char *
mt_ide_model(unsigned minor)
{
	if ( !check(minor) )
		return NULL;
	ide_device *device = get_device(minor);
	return device->present ? device->model : NULL;
}
// Devuelve la capacidad del disco en sectores, o 0 si no está presente
unsigned
mt_ide_capacity(unsigned minor)
{
	if ( !check(minor) )
		return 0;
	ide_device *device = get_device(minor);
	return device->present ? device->capacity : 0;
}
