#include <kernel.h>
#include <apps.h>

#define CLOCKIRQ		0						/* interrupcion de timer */
#define INTFL			0x200					/* bit de habilitación de interrupciones en los flags */
#define MSPERTICK 		10						/* 100 Hz */
#define QUANTUM			10						/* 100 mseg */

Task_t * volatile mt_curr_task;					/* tarea en ejecucion */
Task_t * volatile mt_last_task;					/* tarea anterior */
Task_t * volatile mt_fpu_task;					/* tarea que tiene el coprocesador */

static Time_t volatile timer_ticks;				/* ticks ocurridos desde el arranque */
static unsigned usec_counts;					/* cuentas de delay por microsegundo */
static volatile unsigned ticks_to_run;			/* ranura de tiempo */
static TaskQueue_t ready_q;						/* cola de tareas ready */
static TaskQueue_t terminated_q;				/* cola de tareas terminadas */

static Task_t *task_list;						/* lista de tareas existentes */
static unsigned num_tasks;						/* cantidad de tareas existentes */

static void scheduler(void);

static void block(Task_t *task, TaskState_t state);
static void ready(Task_t *task, bool success);

static unsigned msecs_to_ticks(unsigned msecs);
static unsigned ticks_to_msecs(unsigned ticks);

static void count_down(volatile unsigned *cnt);	/* lazo para hacer delays en microsegundos */

static void free_terminated(void);				/* libera tareas terminadas */
static void clockint(unsigned irq);				/* manejador interrupcion de timer */

static void task_list_add(Task_t *task);		/* agregar a la lista de tareas existentes */
static void task_list_remove(Task_t *task);		/* quitar de la lista de tareas existentes */

static int null_task(void *arg);				/* tarea nula */
static int run_shell(void *arg);				/* tarea que dispara un shell repetidamente */

// Stackframe inicial de una tarea
typedef struct
{
	mt_regs_t		regs;						// registros empujados por una interrupción
	void			(*retaddr)(void);			// dirección de retorno del wrapper (no se usa)
	TaskFunc_t		func;						// función de la tarea
	void *			arg;						// argumento pasado a la tarea
}
InitialStack_t;

// Stackframe para terminar de una tarea
typedef struct
{
	mt_regs_t		regs;						// registros empujados por una interrupción
	void			(*retaddr)(void);			// dirección de retorno de Exit (no se usa)
	int				status;						// argumento pasado a Exit()
}
DeleteStack_t;

// Información que el bootloader pasa al kernel
struct boot_info_t
{
	unsigned		flags;
	unsigned		lowmem_kb;
	unsigned		himem_kb;
	/* etc */
};

/*
--------------------------------------------------------------------------------
mt_main - Inicializacion del kernel

El control viene aquí inmediatamente después del arranque en kstart.S.
Las interrupciones están deshabilitadas, puede usarse el stack y los
registros de segmento CS y DS apuntan a memoria plana con base 0, pueden 
usarse pero	no cargarse, ni siquiera con los mismos valores que tienen. 
--------------------------------------------------------------------------------
*/

void
mt_main(unsigned magic, boot_info_t *info)
{
	unsigned i;
	Task_t *t;

	// Esto funciona aunque el módulo de consola no esté inicializado
	mt_cons_clear();
	print0("*** MTask version %s ***\n", MTASK_VERSION);

	// Inicializar GDT, IDT y registros de segmento
	print0("Inicializando GDT e IDT\n");
	mt_setup_gdt_idt();

	// Inicializar el heap pasándole el tamaño de la memoria disponible por
	// encima de 1 MB según lo informa el bootloader.
	print0("Inicializando heap. Memoria superior: %u kB\n", info->himem_kb);
	mt_setup_heap(info->himem_kb * 1024);

	// Inicializar sistema de interrupciones
	print0("Configurando interrupciones y excepciones\n");
	mt_setup_interrupts();

	// Configurar el timer, colocar el manejador de interrupción
	// correspondiente y habilitar la interrupción
	print0("Configurando timer: %u ms/tick\n", MSPERTICK);
	mt_setup_timer(MSPERTICK);
	mt_set_int_handler(CLOCKIRQ, clockint);
	mt_enable_irq(CLOCKIRQ);

	// Inicializar el sistema de manejo del coprocesador aritmético
	print0("Inicializando manejo del coprocesador\n");
	mt_setup_math();

	// Inicializar la tarea actual
	print0("Inicializando la tarea actual\n");
	mt_curr_task = malloc(sizeof(Task_t));
	memset(mt_curr_task, 0, sizeof(Task_t));
	mt_curr_task->send_queue.name = "init";
	mt_curr_task->state = TaskCurrent;
	mt_curr_task->priority = DEFAULT_PRIO;
	mt_curr_task->protected = true;

	// Iniciar tarea nula. Va a ser la primera en la lista de tareas.
	print0("Crear y correr la tarea nula\n");
	t = CreateTask(null_task, 0, NULL, "idle", MIN_PRIO);
	Protect(t);
	Ready(t);

	// Habilitar interrupciones. 
	print0("Interrupciones habilitadas\n");
	mt_sti();

	// Calibrar lazo de delay para UDelay()
	print0("Calibrando UDelay()\n"); 
	i = msecs_to_ticks(1000);
	while ( timer_ticks < i )
	{
		unsigned n = 1000000;
		count_down(&n);
		usec_counts++;
	}

	// Inicializar drivers.
	print0("Inicializando drivers\n");
	mt_init_drivers();

	// Agregar la tarea actual a la lista de tareas
	task_list_add(mt_curr_task);

	// Disparar shells en todas las consolas menos la 1
	print0("Creando shells iniciales\n");
	for ( i = 2 ; i < NVCONS ; i++ )
	{
		t = CreateTask(run_shell, MAIN_STKSIZE, NULL, "shell", DEFAULT_PRIO);
		Protect(t);
		SetConsole(t, i);
		Ready(t);
	}

	// Fin de la inicialización
	print0("*** MTask inicializado ***\n");

	// Pasarse a la consola 1, tomar el foco y ejecutar un shell
	mt_curr_task->send_queue.name = "shell";
	SetConsole(mt_curr_task, 1);
	Atomic();
	mt_input_setfocus(1);
	Unatomic();
	run_shell(NULL);
}

/*
--------------------------------------------------------------------------------
mt_select_task - determina la próxima tarea a ejecutar.

Retorna true si ha cambiado la tarea en ejecucion.
Llamada desde scheduler() y cuanto retorna una interrupcion de primer nivel.
Si la tarea actual no es dueña del coprocesador, levanta el bit TS en CR0 para que 
se genere la excepción 7 la próxima vez que se ejecute una instrucción de 
coprocesador.
Guarda y restaura el contexto propio del usuario, si existe. 
--------------------------------------------------------------------------------
*/

bool 
mt_select_task(void)
{
	Task_t *ready_task;

	/* Ver si la tarea actual puede conservar la CPU */
	if ( mt_curr_task->state == TaskCurrent )
	{
		if ( mt_curr_task->atomic_level )		/* No molestar */
			return false;

		/* Analizar prioridades y ranura de tiempo */
		ready_task = mt_peeklast(&ready_q);
		if ( !ready_task || ready_task->priority < mt_curr_task->priority ||
			(ticks_to_run && ready_task->priority == mt_curr_task->priority) )
			return false; 

		/* La tarea actual pierde la CPU */
		mt_curr_task->state = TaskReady;
		mt_enqueue(mt_curr_task, &ready_q);
	}

	/* Obtener la próxima tarea, si es la misma no hay nada que hacer */
	ready_task = mt_getlast(&ready_q);
	ready_task->state = TaskCurrent;
	if ( ready_task == mt_curr_task )
		return false;

	/* Guardar contexto adicional */
	if ( mt_curr_task->save )
		mt_curr_task->save();

	/* Guardar TLS */
	mt_curr_task->tls = TLS;

	/* Cambiar la tarea actual */
	mt_last_task = mt_curr_task;
	mt_curr_task = ready_task;

	/* Si la tarea actual es dueña del coprocesador aritmético,
	   bajar el bit TS en CR0. En caso contrario, levantarlo para que
	   la próxima instrucción de coprocesador genere una excepción 7 */
	if ( mt_curr_task == mt_fpu_task )
		mt_clts();
	else
		mt_stts();

	/* Inicializar ranura de tiempo */
	ticks_to_run = QUANTUM;

	/* Reponer TLS */
	TLS = mt_curr_task->tls;

	/* Actualizar terminal actual */
	mt_input_setcurrent(mt_curr_task->consnum);

	/* Reponer contexto adicional */
	if ( mt_curr_task->restore )
		mt_curr_task->restore();

	return true;
}

/* Funciones internas */

/*
--------------------------------------------------------------------------------
run_shell - tarea que ejecuta repetidamente un shell
--------------------------------------------------------------------------------
*/

static int
run_shell(void *arg)
{
	char *argv[] = { "shell", NULL };	
	while ( true )
	{
		mt_cons_clear();
		cprintk(LIGHTCYAN, BLACK, "Bienvenido a MTask\n");
		shell_main(1, argv);
	}
	return 0;
}

/*
--------------------------------------------------------------------------------
msecs_to_ticks, ticks_to_msecs - conversion de milisegundos a ticks y viceversa
--------------------------------------------------------------------------------
*/

static unsigned 
msecs_to_ticks(unsigned msecs)
{
	return (msecs + MSPERTICK - 1) / MSPERTICK;
}

static unsigned 
ticks_to_msecs(unsigned ticks)
{
	return ticks * MSPERTICK;
}

/*
--------------------------------------------------------------------------------
count_down - lazo contador para hacer pequeños delays en microsegundos
--------------------------------------------------------------------------------
*/

static void
count_down(volatile unsigned *cnt)
{
	while ( (*cnt)-- )
		;
}

/*
--------------------------------------------------------------------------------
block - bloquea una tarea desencolándola y poniéndola en un estado.
--------------------------------------------------------------------------------
*/

static void
block(Task_t *task, TaskState_t state)
{
	mt_dequeue(task);
	mt_dequeue_time(task);
	task->state = state;
}

/*
--------------------------------------------------------------------------------
ready - desbloquea una tarea y la pone en la cola de ready

Si la tarea estaba bloqueada en WaitQueue, Send o Receive, el argumento
success determina el status de retorno de la funcion que la bloqueo.
--------------------------------------------------------------------------------
*/

static void
ready(Task_t *task, bool success)
{
	if ( task->state == TaskReady )
		return;

	mt_dequeue(task);
	mt_dequeue_time(task);
	mt_enqueue(task, &ready_q);
	task->success = success;
	task->state = TaskReady;
}

/*
--------------------------------------------------------------------------------
free_terminated - elimina las tareas terminadas.
--------------------------------------------------------------------------------
*/

static void
free_terminated(void)
{
	Task_t *task;
	char *name;

	while ( true )
	{
		Atomic();
		task = mt_getlast(&terminated_q);
		Unatomic();
		if ( !task )
			break;
		Atomic();	
		if ( (name = GetName(task)) )
			free(name);
		free(task->stack);
		if ( task->math_data )
			free(task->math_data);
		free(task);
		Unatomic();
	}
}

/*
--------------------------------------------------------------------------------
scheduler - selecciona la próxima tarea a ejecutar y cambia contexto.

Se llama cuando se bloquea la tarea actual o se despierta cualquier tarea.
No hace nada si se llama desde una interrupcion, porque las interrupciones
pueden despertar tareas pero recien se cambia contexto al retornar de la
interrupcion de primer nivel.
--------------------------------------------------------------------------------
*/

static void
scheduler(void)
{
	if ( !mt_int_level && mt_select_task() )
		mt_context_switch();
}

/*
--------------------------------------------------------------------------------
clockint - interrupcion de tiempo real

Despierta a las tareas de la cola de tiempo que tengan su cuenta de ticks
agotada, y decrementa la cuenta de la primera que quede en la cola.
Decrementa la ranura de tiempo de la tarea actual.
--------------------------------------------------------------------------------
*/

static void 
clockint(unsigned irq)
{
	Task_t *task;

	++timer_ticks;
	if ( ticks_to_run )
		ticks_to_run--;
	while ( (task = mt_peekfirst_time()) && !task->ticks )
	{
		mt_getfirst_time();
		ready(task, false);
	}
	if ( task )
		task->ticks--;
}

/*
--------------------------------------------------------------------------------
null_task - Tarea nula

Corre con prioridad 0 y toma la CPU cuando ninguna otra tarea pueda ejecutar.
Limpia la cola de tareas terminadas y detiene el procesador.
--------------------------------------------------------------------------------
*/

static int
null_task(void *arg)
{
	while ( true )
	{
		free_terminated();
		mt_hlt();
	}
	return 0;
}

/*
--------------------------------------------------------------------------------
task_list_add - Agregar una tarea al principio de la lista de tareas.

La tarea se pone en la cabeza de la lista. Acaba de ser creada y viene con los 
punteros a próximo y siguiente en NULL. La primera tarea que se inserta es la 
tarea nula, que nunca se quita. Por lo tanto, será siempre la última. 
--------------------------------------------------------------------------------
*/

static void 
task_list_add(Task_t *task)
{
	num_tasks++;

	if ( !task_list )
	{
		// Primera inserción, tarea nula
		task_list = task;
		return;
	}

	// Insertar en la cabeza
	task->list_next = task_list;
	task_list->list_prev = task;
	task_list = task;
}

/*
--------------------------------------------------------------------------------
task_list_remove - Quitar una tarea de la lista de tareas.

La lista no está vacía y la tarea que se va a sacar no es la última (lugar 
ocupado por la tarea nula).
--------------------------------------------------------------------------------
*/

static void 
task_list_remove(Task_t *task)
{
	num_tasks--;

	if ( task == task_list )
	{
		// Primera tarea de la lista
		task_list = task_list->list_next;
		task_list->list_prev = NULL;
		return;
	}

	// Ni la primera ni la última
	task->list_prev->list_next = task->list_next;
	task->list_next->list_prev = task->list_prev;
}

/*
--------------------------------------------------------------------------------
wrapper - ejecuta el cuerpo de una tarea y llama a Exit() con su status de salida
--------------------------------------------------------------------------------
*/

static void
wrapper(TaskFunc_t func, void *arg)
{
	Exit(func(arg));
}

/* API */

/*
--------------------------------------------------------------------------------
CreateTask - crea una tarea.

Recibe un puntero a una funcion de tipo void f(void*), tamano del stack,
un puntero para pasar como argumento, nombre, prioridad inicial.
Toma memoria para crear el stack y lo inicializa para que retorne
a Exit().
Está inicialmente suspendida, para ejecutarla llamar a Ready().
--------------------------------------------------------------------------------
*/

Task_t *
CreateTask(TaskFunc_t func, unsigned stacksize, void *arg, const char *name, unsigned priority)
{
	Task_t *task;
	InitialStack_t *s;

	/* alocar bloque de control */
	task = Malloc(sizeof(Task_t));
	task->send_queue.name = StrDup(name);
	task->priority = priority;
	task->tls = TLS;							// hereda TLS actual
	task->consnum = mt_curr_task->consnum;		// hereda número de consola

	/* alocar stack */
	if ( stacksize < MIN_STACK )				// garantizar tamaño mínimo
		stacksize = MIN_STACK;
	else
		stacksize &= ~3;						// redondear a multiplos de 4
	task->stack = Malloc(stacksize);			// malloc alinea adecuadamente

	/* 
	Inicializar el stack simulando que wrapper(func, arg) fue interrumpida 
	antes de ejecutar su primera instrucción y empujó su contexto al stack. 
	Inicializamos eflags para que corra con interrupciones habilitadas y 
	apuntamos cs:eip a su primera instrucción. Son los registros que el i386
	empuja al stack cuando se produce una interrupción. Los demás carecen 
	de importancia, porque wrapper() todavía no comenzó a ejecutar.
	*/
	s = (InitialStack_t *)(task->stack + stacksize) - 1;

	s->regs.eip = (unsigned) wrapper;			// simular interrupción
	s->regs.cs = MT_CS;							// .
	s->regs.eflags = INTFL;						// .
	s->func = func;								// primer argumento de wrapper
	s->arg = arg;								// segundo argumento de wrapper

	task->esp = &s->regs;						// puntero a stack inicial

	// Agregar a lista de tareas
	Atomic();
	task_list_add(task);
	Unatomic();

	return task;
}

/*
--------------------------------------------------------------------------------
DeleteTask - elimina una tarea creada con CreateTask.

Si es la tarea actual ejecuta Exit(), en caso contrario se modifica su 
contexto para que ejecute Exit() la próxima vez que recupere el contexto,
con interrupciones habilitadas y en modo preemptivo. Si está bloqueada se
la despierta, haciendo fracasar una posible función bloqueante.
--------------------------------------------------------------------------------
*/

bool
DeleteTask(Task_t *task, int status)
{
	if ( task == mt_curr_task )
		Exit(status);

	if ( !mt_curr_task->protected && task->protected )
		return false;
	bool ints = SetInts(false);
	task->esp->eip = (unsigned) Exit;				// la tarea va a ejecutar Exit() 
	task->atomic_level = 0;							// en modo preemptivo
	task->esp->eflags = INTFL;						// con interrupciones habilitadas
	((DeleteStack_t *)task->esp)->status = status;	// argumento de Exit()
	ready(task, false);
	scheduler();
	SetInts(ints);
	return true;
}

/*
--------------------------------------------------------------------------------
Protect - protege una tarea contra DeleteTask()
--------------------------------------------------------------------------------
*/

bool
Protect(Task_t *task)
{
	if ( !mt_curr_task->protected )
		return false;
	task->protected = true;
	return true;
}

/*
--------------------------------------------------------------------------------
Attach - vincular una tarea a la actual
Detach - desvincular una tarea vinculada a la actual
--------------------------------------------------------------------------------
*/

bool
Attach(Task_t *task)
{
	if ( !mt_curr_task->protected && task->protected )
		return false;
	Atomic();
	if ( task->attached_to || task->exiting )
	{
		Unatomic();
		return false;
	}
	mt_curr_task->nattached++;
	task->attached_to = mt_curr_task;
	Unatomic();
	return true;
}

bool
Detach(Task_t *task)
{
	if ( task->attached_to != mt_curr_task )
		return false;
	bool ints = SetInts(false);
	mt_curr_task->nattached--;
	if ( task->state == TaskZombie )
	{
		ready(task, true);
		scheduler();
	}
	else
		task->attached_to = NULL;
	SetInts(ints);
	return true;
}


/*
--------------------------------------------------------------------------------
SetPriority - establece la prioridad de una tarea

Si la tarea estaba en una cola, la desencola y la vuelve a encolar para
reflejar el cambio de prioridad en su posición en la cola.
Si se le ha cambiado la prioridad a la tarea actual o a una que esta ready se
llama al scheduler.
--------------------------------------------------------------------------------
*/

bool		
SetPriority(Task_t *task, unsigned priority)
{
	TaskQueue_t *queue;

	if ( !mt_curr_task->protected && task->protected )
		return false;
	bool ints = SetInts(false);
	task->priority = priority;
	if ( (queue = task->queue) )		// re-encolar según la nueva prioridad
	{
		mt_dequeue(task);
		mt_enqueue(task, queue);
	}
	if ( task == mt_curr_task || task->state == TaskReady )
		scheduler();
	SetInts(ints);
	return true;
}

/*
--------------------------------------------------------------------------------
SetConsole - establece el número de terminal de la tarea.
--------------------------------------------------------------------------------
*/

bool
SetConsole(Task_t *task, unsigned consnum)
{
	if ( consnum >= NVCONS || (!mt_curr_task->protected && task->protected) )
		return false;
	bool ints = SetInts(false);
	if ( task->consnum != consnum )
	{
		task->consnum = consnum;
		if ( task == mt_curr_task )
			mt_input_setcurrent(consnum);
	}
	SetInts(ints);
	return true;
}

/*
--------------------------------------------------------------------------------
SetSaveRestore - establece callbacks para guardar y reponer contexto adicional.
--------------------------------------------------------------------------------
*/

bool
SetSaveRestore(Task_t *task, SaveRestore_t save, SaveRestore_t restore)
{
	if ( !mt_curr_task->protected && task->protected )
		return false;
	Atomic();
	task->save = save;
	task->restore = restore;
	Unatomic();
	return true;
}

/*
--------------------------------------------------------------------------------
SetCleanup - establece un callback para ejecutar cuando la tarea termina.
--------------------------------------------------------------------------------
*/

bool
SetCleanup(Task_t *task, Cleanup_t cleanup)
{
	if ( !mt_curr_task->protected && task->protected )
		return false;
	task->cleanup = cleanup;
	return true;
}

/*
--------------------------------------------------------------------------------
GetInfo - devuelve información sobre una tarea
--------------------------------------------------------------------------------
*/

void
GetInfo(Task_t *task, TaskInfo_t *info)
{
	bool ints = SetInts(false);
	info->task = task;
	info->consnum = task->consnum;
	info->priority = task->priority;
	switch ( info->state = task->state )
	{
		case TaskWaiting:
		case TaskSending: 
			info->waiting = task->queue;
			break;
		case TaskReceiving: 
			info->waiting = task->from;
			break;
		case TaskJoining:
			info->waiting = task->join;
			break;
		case TaskZombie:
			info->waiting = task->attached_to;
			break;
		default:
			info->waiting = NULL;
			break;
	}
	info->is_timeout = task->in_time_q;
	info->timeout = ticks_to_msecs(task->ticks);
	info->protected = task->protected;
	SetInts(ints);
}

/*
--------------------------------------------------------------------------------
GetTasks - devuelve información sobre las tareas existentes

Retorna un array de estructuras de tipo TaskInfo_t, una por tarea, y la cantidad
de tareas. El array está alocado dinámicamente, liberar llamando a Free().
--------------------------------------------------------------------------------
*/

TaskInfo_t *
GetTasks(unsigned *ntasks)
{
	Task_t *t;
	TaskInfo_t *ti, *info;

	Atomic();
	*ntasks = num_tasks;
	for ( t = task_list, ti = info = Malloc(num_tasks * sizeof(TaskInfo_t)) ; t ; t = t->list_next, ti++ )
		GetInfo(t, ti);
	Unatomic();
	return info;
}

/*
--------------------------------------------------------------------------------
Ready - pone una tarea en la cola ready
--------------------------------------------------------------------------------
*/

bool
Ready(Task_t *task)
{
	if ( !mt_curr_task->protected && task->protected )
		return false;
	bool ints = SetInts(false);
	ready(task, false);
	scheduler();
	SetInts(ints);
	return true;
}

/*
--------------------------------------------------------------------------------
Suspend - suspende una tarea
--------------------------------------------------------------------------------
*/

bool
Suspend(Task_t *task)
{
	if ( !mt_curr_task->protected && task->protected )
		return false;
	bool ints = SetInts(false);
	block(task, TaskSuspended);
	if ( task == mt_curr_task )
		scheduler();
	SetInts(ints);
	return true;
}

/*
--------------------------------------------------------------------------------
CurrentTask - retorna la tarea actual
--------------------------------------------------------------------------------
*/

Task_t *
CurrentTask(void)
{
	return mt_curr_task;
}

/*
--------------------------------------------------------------------------------
Pause - suspende la tarea actual
--------------------------------------------------------------------------------
*/

void
Pause(void)
{
	Suspend(mt_curr_task);
}

/*
--------------------------------------------------------------------------------
Yield - cede voluntariamente la CPU
--------------------------------------------------------------------------------
*/

void
Yield(void)
{
	Ready(mt_curr_task);
}

/*
--------------------------------------------------------------------------------
Delay - pone a la tarea actual a dormir durante una cantidad de milisegundos
--------------------------------------------------------------------------------
*/

void
Delay(unsigned msecs)
{
	bool ints = SetInts(false);
	if ( msecs )
	{
		block(mt_curr_task, TaskDelaying);
		if ( msecs != FOREVER )
			mt_enqueue_time(mt_curr_task, msecs_to_ticks(msecs));
	}
	else
		ready(mt_curr_task, false);
	scheduler();
	SetInts(ints);
}

/*
--------------------------------------------------------------------------------
UDelay - hace un pequeño delay en microsegundos
--------------------------------------------------------------------------------
*/

void
UDelay(unsigned usecs)
{
	while ( usecs-- )
	{
		unsigned n = usec_counts;
		count_down(&n);
	}
}

/*
--------------------------------------------------------------------------------
Join, JoinCond, JoinTimed - Esperar que termine una tarea vinculada a la actual
--------------------------------------------------------------------------------
*/

bool 
Join(Task_t *task, int *status)
{
	return JoinTimed(task, status, FOREVER);
}

bool 
JoinCond(Task_t *task, int *status)
{
	return JoinTimed(task, status, 0);
}

bool
JoinTimed(Task_t *task, int *status, unsigned msecs)
{
	bool success;
	bool ints = SetInts(false);

	if ( task->attached_to != mt_curr_task )	// no está vinculada nosotros
	{
		SetInts(ints);
		return false;
	}

	if ( task->state == TaskZombie )			// está esperando nuestro Join()
	{
		*status = task->join_status;
		mt_curr_task->nattached--;
		ready(task, true);
		scheduler();
		SetInts(ints);
		return true;
	}

	// Si el timeout es nulo, fracasar inmediatamente
	if ( !msecs )
	{
		SetInts(ints);
		return false;
	}

	// Bloquearse hasta que la tarea termine
	mt_curr_task->join = task;
	mt_curr_task->state = TaskJoining;
	if ( msecs != FOREVER )
		mt_enqueue_time(mt_curr_task, msecs_to_ticks(msecs));
	scheduler();
	if ( (success = mt_curr_task->success) )
	{
		*status = mt_curr_task->join_status;
		mt_curr_task->nattached--;
	}
	SetInts(ints);
	return success;		
}

/*
--------------------------------------------------------------------------------
Exit - finaliza la tarea actual

Todas las tareas creadas con CreateTask retornan a esta funcion que las mata.
Esta funcion nunca retorna. Ejecuta un manejador de cleanup si ha sido instalado.
La tarea ingresa en la cola de tareas terminadas, para su posterior limpieza.
--------------------------------------------------------------------------------
*/

void
Exit(int status)
{
	Task_t *t;

	if ( mt_curr_task->exiting )
		Panic ("Exit recursivo");

	mt_curr_task->exiting = true;

	if ( mt_curr_task->cleanup )
		mt_curr_task->cleanup();					// no debe llamar a Exit()

	Atomic();
	if ( mt_curr_task->nattached )					// desvincular tareas vinculadas
		for ( t = task_list ; t ; t = t->list_next )
			if ( t->attached_to == mt_curr_task )
			{					
				bool ints = SetInts(false);
				t->attached_to = NULL;
				if ( t->state == TaskZombie )
					ready(t, true);
				SetInts(ints);
			}
	mt_cli();
	if ( (t = mt_curr_task->attached_to) )			// estamos vinculados
	{
		if ( t->state == TaskJoining && t->join == mt_curr_task )
		{
			// t ya ejecutó Join() y nos está esperando
			t->join_status = status;
			ready(t, true);
		}
		else
			do
			{
				// bloquearse hasta que t ejecute Join()
				mt_curr_task->state = TaskZombie;
				mt_curr_task->join_status = status;
				scheduler();
			}
			while ( !mt_curr_task->success );
	}
	FlushQueue(&mt_curr_task->send_queue, false);	// no recibimos más mensajes
	if ( mt_fpu_task == mt_curr_task )				// liberar el coprocesador
		mt_fpu_task = NULL;
	task_list_remove(mt_curr_task);					// quitar de la lista de tareas
	mt_curr_task->state = TaskTerminated;			// terminar
	mt_curr_task->priority = 0;						// para encolar más rápido
	mt_enqueue(mt_curr_task, &terminated_q);		// al recolector de basura
	scheduler();									// no retorna
}

/*
--------------------------------------------------------------------------------
Panic - error fatal del sistema

Deshabilita interrupciones, pone el foco en la consola de la tarea actual,
imprime mensaje de error y detiene el sistema.
--------------------------------------------------------------------------------
*/

void
Panic(const char *format, ...)
{
	va_list ap;

	mt_cli();
	mt_cons_setfocus(mt_curr_task->consnum);
	mt_cons_setattr(WHITE, BLUE);
	mt_cons_cursor(false);
	mt_cons_cr();
	mt_cons_nl();
	printk("PANIC: %s (cons %u)", GetName(mt_curr_task), mt_curr_task->consnum);
	mt_cons_clreol();
	mt_cons_cr();
	mt_cons_nl();
	va_start(ap, format);
	vprintk(format, ap);
	va_end(ap);
	mt_cons_clreol();
	mt_hlt();
}

/*
--------------------------------------------------------------------------------
Atomic - deshabilita el modo preemptivo para la tarea actual (anidable)
--------------------------------------------------------------------------------
*/

void
Atomic(void)
{
	++mt_curr_task->atomic_level;
}

/*
--------------------------------------------------------------------------------
Unatomic - habilita el modo preemptivo para la tarea actual (anidable)
--------------------------------------------------------------------------------
*/

void
Unatomic(void)
{
	if ( mt_curr_task->atomic_level && !--mt_curr_task->atomic_level )
	{
		bool ints = SetInts(false);
		scheduler();
		SetInts(ints);
	}
}

/*
--------------------------------------------------------------------------------
SetInts - habilita o deshabilita interrupciones para la tarea actual, y devuelve
	el valor anterior de la habilitación de interrupciones.
--------------------------------------------------------------------------------
*/

bool
SetInts(bool enabled)
{
	unsigned flags = mt_flags();
	if ( enabled )
		mt_sti();
	else
		mt_cli();
	return (flags & INTFL) != 0;
}

/*
--------------------------------------------------------------------------------
TLS - puntero a datos locales de la tarea actual
--------------------------------------------------------------------------------
*/

void *TLS;

/*
--------------------------------------------------------------------------------
CreateQueue - crea una cola de tareas
--------------------------------------------------------------------------------
*/

TaskQueue_t *	
CreateQueue(const char *name)
{
	TaskQueue_t *queue = Malloc(sizeof(TaskQueue_t));

	queue->name = StrDup(name);
	return queue;
}

/*
--------------------------------------------------------------------------------
DeleteQueue - destruye una cola de tareas
--------------------------------------------------------------------------------
*/

void
DeleteQueue(TaskQueue_t *queue)
{
	FlushQueue(queue, false);
	Free(queue->name);
	Free(queue);
}

/*
--------------------------------------------------------------------------------
WaitQueue, WaitQueueCond, WaitQueueTimed - esperar en una cola de tareas

El valor de retorno es true si la tarea fue despertada por SignalQueue
o el valor pasado a FlushQueue.
Si msecs es FOREVER, espera indefinidamente. Si msecs es cero, retorna false.
--------------------------------------------------------------------------------
*/

bool			
WaitQueue(TaskQueue_t *queue)
{
	return WaitQueueTimed(queue, FOREVER);
}

bool			
WaitQueueTimed(TaskQueue_t *queue, unsigned msecs)
{
	bool success;

	if ( !msecs )
		return false;

	bool ints = SetInts(false);
	block(mt_curr_task, TaskWaiting);
	mt_enqueue(mt_curr_task, queue);
	if ( msecs != FOREVER )
		mt_enqueue_time(mt_curr_task, msecs_to_ticks(msecs));
	scheduler();
	success = mt_curr_task->success;
	SetInts(ints);

	return success;
}

/*
--------------------------------------------------------------------------------
SignalQueue, FlushQueue - funciones para despertar tareas en una cola

SignalQueue despierta la última tarea de la cola (la de mayor prioridad o
la que llegó primero entre dos de la misma prioridad) y devuelve un puntero a la
misma (o NULL si la cola estaba vacía). La tarea despertada completa su 
WaitQueue() exitosamente.
FlushQueue despierta a todas las tareas de la cola, que completan su
WaitQueue() con el resultado que se pasa como argumento.
--------------------------------------------------------------------------------
*/

Task_t *		
SignalQueue(TaskQueue_t *queue)
{
	Task_t *task;

	bool ints = SetInts(false);
	if ( (task = mt_getlast(queue)) )
	{
		ready(task, true);
		scheduler();
	}
	SetInts(ints);

	return task;
}

void			
FlushQueue(TaskQueue_t *queue, bool success)
{
	Task_t *task;

	bool ints = SetInts(false);
	if ( mt_peeklast(queue) )
	{
		while ( (task = mt_getlast(queue)) )
			ready(task, success);
		scheduler();
	}
	SetInts(ints);
}

/*
--------------------------------------------------------------------------------
Send, SendCond, SendTimed - enviar un mensaje
--------------------------------------------------------------------------------
*/

bool			
Send(Task_t *to, void *msg, unsigned size)
{
	return SendTimed(to, msg, size, FOREVER);
}

bool			
SendCond(Task_t *to, void *msg, unsigned size)
{
	return SendTimed(to, msg, size, 0);
}

bool			
SendTimed(Task_t *to, void *msg, unsigned size, unsigned msecs)
{
	bool success;

	bool ints = SetInts(false);

	if ( to->state == TaskReceiving && (!to->from || to->from == mt_curr_task) )
	{
		to->from = mt_curr_task;
		if ( to->msg && msg )
		{
			if ( size > to->size )
				Panic("Buffer insuficiente para transmitir mensaje a %s, %u > %u", GetName(to), size, to->size);
			to->size = size;
			memcpy(to->msg, msg, size);
		}
		else
			to->size = 0;
		ready(to, true);
		scheduler();
		SetInts(ints);
		return true;
	}

	if ( !msecs )
	{
		SetInts(ints);
		return false;
	}

	mt_curr_task->msg = msg;
	mt_curr_task->size = size;
	mt_curr_task->state = TaskSending;
	mt_enqueue(mt_curr_task, &to->send_queue);
	if ( msecs != FOREVER )
		mt_enqueue_time(mt_curr_task, msecs_to_ticks(msecs));
	scheduler();
	success = mt_curr_task->success;

	SetInts(ints);
	return success;
}


/*
--------------------------------------------------------------------------------
Receive, ReceiveCond, ReceiveTimed - recibir un mensaje
--------------------------------------------------------------------------------
*/

bool			
Receive(Task_t **from, void *msg, unsigned *size)
{
	return ReceiveTimed(from, msg, size, FOREVER);
}

bool			
ReceiveCond(Task_t **from, void *msg, unsigned *size)
{
	return ReceiveTimed(from, msg, size, 0);
}

bool			
ReceiveTimed(Task_t **from, void *msg, unsigned *size, unsigned msecs)
{
	bool success;
	Task_t *sender;

	bool ints = SetInts(false);

	if ( from && *from )
		sender = (*from)->queue == &mt_curr_task->send_queue ? *from : NULL;
	else
		sender = mt_peeklast(&mt_curr_task->send_queue);

	if ( sender )
	{
		if ( from ) 
			*from = sender;
		if ( sender->msg && msg )
		{
			if ( size )
			{
				if ( sender->size > *size )
					Panic("Buffer insuficiente para recibir mensaje de %s, %u > %u", GetName(sender), sender->size, *size);
				memcpy(msg, sender->msg, *size = sender->size);
			}
		}
		else if ( size )
			*size = 0;
		ready(sender, true);
		scheduler();
		SetInts(ints);
		return true;
	}

	if ( !msecs )
	{
		SetInts(ints);
		return false;
	}

	mt_curr_task->from = from ? *from : NULL;
	mt_curr_task->msg = msg;
	mt_curr_task->size = size ? *size : 0;
	mt_curr_task->state = TaskReceiving;
	if ( msecs != FOREVER )
		mt_enqueue_time(mt_curr_task, msecs_to_ticks(msecs));
	scheduler();
	if ( (success = mt_curr_task->success) )
	{
		if ( size )
			*size = mt_curr_task->size;
		if ( from )
			*from = mt_curr_task->from;
	}

	SetInts(ints);
	return success;
}

/*
--------------------------------------------------------------------------------
GetName - devuelve el nombre de cualquier objeto creado mediante una función
	Create...(), incluyendo tareas.
--------------------------------------------------------------------------------
*/

char *
GetName(void *object)
{
	return object ? *(char **)object : NULL;
}

/*
--------------------------------------------------------------------------------
Time - devuelve los milisegundos transcurridos desde el arranque 
--------------------------------------------------------------------------------
*/

Time_t
Time(void)
{
	return ticks_to_msecs(timer_ticks);
}

/*
--------------------------------------------------------------------------------
Malloc, StrDup, Free - manejo de memoria dinamica
--------------------------------------------------------------------------------
*/

void *
Malloc(unsigned size)
{
	void *p;

	free_terminated();
	Atomic();
	if ( !(p = malloc(size)) )
		Panic("Error malloc");
	Unatomic();
	memset(p, 0, size);
	return p;
}

char *
StrDup(const char *str)
{
	char *p;

	if ( !str )
		return NULL;
	free_terminated();
	Atomic();
	if ( !(p = malloc(strlen(str) + 1)) )
		Panic("Error strdup");
	Unatomic();
	strcpy(p, str);
	return p;
}

void
Free(void *mem)
{
	if ( !mem )
		return;
	Atomic();
	free(mem);
	Unatomic();
}

