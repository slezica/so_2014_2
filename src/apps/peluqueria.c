#include <kernel.h>
	
#define NCHAIRS 5
#define NBARBERS 3

#define COLOR_SLEEPING LIGHTRED
#define COLOR_CUTTING WHITE
#define COLOR_FREE WHITE
#define COLOR_STATUS YELLOW
#define COLOR_PROMPT LIGHTCYAN
#define COLOR_BG BLUE
#define SLEEPING "ZZZ..."
#define FREE_CHAIR "--"

static const unsigned x_left = 20, x_right = 50;
static const unsigned y_chairs[] = { 7, 9, 11, 13, 15 };
static const unsigned y_barb[] = { 8, 10, 12 };
static const unsigned y_title = 5, y_status = 17, y_prompt = 19;

// TLS

typedef struct
{
	int client_id, barber_id, nclients;
	Monitor_t *mon;
	int waiting[NCHAIRS];
	Condition_t *clients_waiting, *client_wakeup[NCHAIRS];
	Task_t *barbers[NBARBERS];
}
data;

#define client_id		TLS(data)->client_id
#define barber_id		TLS(data)->barber_id
#define nclients		TLS(data)->nclients
#define mon				TLS(data)->mon
#define waiting			TLS(data)->waiting
#define clients_waiting	TLS(data)->clients_waiting
#define client_wakeup	TLS(data)->client_wakeup
#define barbers			TLS(data)->barbers

static void
print(unsigned x, unsigned y, unsigned fg, unsigned bg, const char *fmt, ...)
{
	char buf[100];
	va_list ap;

	va_start(ap, fmt);
	vsprintf(buf, fmt, ap);
	va_end(ap);

	Atomic();
	mt_cons_gotoxy(x, y);
	cprintk(fg, bg, "%s", buf);
	mt_cons_clreol();
	Unatomic();
}

static int
get_client(void)
{
	int id = 0, chair = -1;
	int i, n;

	/* Para respetar el orden de llegada, recorro las sillas y elijo
	   el cliente con el id más bajo */	
	for ( i = 0 ; i < NCHAIRS ; i++ )
		if ( (n = waiting[i]) && (!id || n < id) )
		{
			id = n;
			chair = i;
		}
	/* Despertamos al cliente */
	if ( chair != -1 )
	{
		waiting[chair] = 0;
		SignalCondition(client_wakeup[chair]);
	}

	return id;
}

static void
barber_sleep(int barb)
{
	print(x_right, y_barb[barb-1], COLOR_SLEEPING, COLOR_BG, "%s", SLEEPING);
}

static void
cut_hair(int barb, int clt)
{
	print(x_right, y_barb[barb-1], COLOR_CUTTING, COLOR_BG, "%u", clt);
	Delay(2000 + rand() % 5000);
}

static int
barber(void *arg)
{
	int id;

	EnterMonitor(mon);
	id = barber_id++;
	LeaveMonitor(mon);

	while ( 1 )
	{
		int n;

		EnterMonitor(mon);
		/* Si hay algún cliente esperando, despertarlo */
		while ( !(n = get_client()) )
		{
			/* Si no, a dormir */
			barber_sleep(id);
			WaitCondition(clients_waiting);
		}
		LeaveMonitor(mon);

		/* Cortar el pelo */
		cut_hair(id, n);

		EnterMonitor(mon);
		nclients--;
		LeaveMonitor(mon);
	}
	return 0;
}

static int 
get_chair(void)
{
	int i;

	/* Buscar una silla libre */
	for ( i = 0 ; i < NCHAIRS ; i++ )
		if ( !waiting[i] )
			return i;
	return -1;
}

static void 
clt_sleep(unsigned clt, unsigned chair)
{
	print(x_left, y_status, COLOR_STATUS, BLACK, "Cliente %u: espero en silla %u", clt, chair);
	print(x_left, y_chairs[chair], COLOR_SLEEPING, COLOR_BG, "%u", clt);
}

static void 
clt_wakeup(unsigned clt, unsigned chair)
{
	print(x_left, y_status, COLOR_STATUS, BLACK, "Cliente %u: me atiende un peluquero", clt);
	print(x_left, y_chairs[chair], COLOR_FREE, COLOR_BG, FREE_CHAIR);
}

static void 
clt_nochair(unsigned clt)
{
	print(x_left, y_status, COLOR_STATUS, BLACK, "Cliente %u: no hay sillas libres", clt);
}

static int
client(void *arg)
{
	int id, n;

	EnterMonitor(mon);
	id = client_id++;

	/* Si hay una silla libre, sentarse */
	if ( (n = get_chair()) != -1 )
	{
		waiting[n] = id;
		/* Eventualmente despertar un peluquero */
		SignalCondition(clients_waiting);
		/* Dormir */
		clt_sleep(id, n);
		WaitCondition(client_wakeup[n]);
		clt_wakeup(id, n);
	}
	else
	/* Si no, irse */
	{
		nclients--;
		clt_nochair(id);
	}

	LeaveMonitor(mon);
	return 0;
}

static Task_t *
create_barber(void)
{
	Task_t *t = CreateTask(barber, 0, NULL, "barber", DEFAULT_PRIO);
	Ready(t);
	return t;
}

static void
create_client(void)
{
	Task_t *t = CreateTask(client, 0, NULL, "client", DEFAULT_PRIO);
	EnterMonitor(mon);
	nclients++;
	LeaveMonitor(mon);
	Ready(t);
}

int
peluqueria_main(int argc, char **argv)
{
	int i, c;
	bool cursor;
	
	mt_cons_clear();
	cursor = mt_cons_cursor(false);
	
	TLS = Malloc(sizeof(data));

	print(x_left, y_title, COLOR_PROMPT, BLACK, "Sillas");
	print(x_right, y_title, COLOR_PROMPT, BLACK, "Peluqueros");
	print(x_left, y_prompt, COLOR_PROMPT, BLACK, "S para salir");
	print(x_left, y_prompt+1, COLOR_PROMPT, BLACK, "Cualquier otra tecla para crear un cliente");

	// Inicializar recursos
	client_id = barber_id = 1;
	nclients = 0;
	memset(waiting, 0, sizeof waiting);
	mon = CreateMonitor("monitor");
	clients_waiting = CreateCondition("clients waiting", mon);
	for ( i = 0 ; i < NCHAIRS ; i++ )
	{
		client_wakeup[i] = CreateCondition("client wakeup", mon);
		print(x_left, y_chairs[i], COLOR_FREE, COLOR_BG, FREE_CHAIR);
	}

	// Crear los peluqueros
	for ( i = 0 ; i < NBARBERS ; i++ )
		barbers[i] = create_barber();

	// Con cada tecla disparar un cliente. Q para terminar.
	while ( (c = getch()) != 'S' && c != 's' )
		create_client();

	// Esperar a que terminen todos los clientes
	print(x_left, y_prompt+2, COLOR_PROMPT, BLACK, "Esperando que terminen los clientes...");
	while ( nclients )
		Yield();
	
	// Liberar recursos
	for ( i = 0 ; i < NBARBERS ; i++ )
		DeleteTask(barbers[i], 0);

	for ( i = 0 ; i < NCHAIRS ; i++ )
		DeleteCondition(client_wakeup[i]);
	
	DeleteMonitor(mon);

	Free(TLS);
	
	mt_cons_clear();
	mt_cons_cursor(cursor);

	return 0;
}
