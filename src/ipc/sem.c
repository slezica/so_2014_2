#include <kernel.h>

struct Semaphore_t
{
	TaskQueue_t		queue;
	unsigned		value;
};

/*
--------------------------------------------------------------------------------
CreateSem - aloca un semaforo y establece su cuenta inicial
--------------------------------------------------------------------------------
*/

Semaphore_t *CreateSem(const char *name, unsigned value)
{
	Semaphore_t *sem = Malloc(sizeof(Semaphore_t));

	sem->queue.name = StrDup(name);
	sem->value = value;
	return sem;
}

/*
--------------------------------------------------------------------------------
DeleteSem - da de baja un semaforo
--------------------------------------------------------------------------------
*/

void
DeleteSem(Semaphore_t *sem)
{
	bool ints = SetInts(false);
	FlushQueue(&sem->queue, false);
	Free(GetName(sem));
	Free(sem);
	SetInts(ints);
}

/*
--------------------------------------------------------------------------------
WaitSem, WaitSemCond, WaitSemTimed - esperar en un semaforo

WaitSem espera indefinidamente, WaitSemCond retorna inmediatamente y
WaitSemTimed espera con timeout. El valor de retorno indica si se consumio
un evento del semaforo.
--------------------------------------------------------------------------------
*/

bool
WaitSem(Semaphore_t *sem)
{
	return WaitSemTimed(sem, FOREVER);
}

bool
WaitSemCond(Semaphore_t *sem)
{
	return WaitSemTimed(sem, 0);
}

bool
WaitSemTimed(Semaphore_t *sem, unsigned msecs)
{
	bool success;

	bool ints = SetInts(false);
	if ( (success = (sem->value > 0)) )
		sem->value--;
	else
		success = WaitQueueTimed(&sem->queue, msecs);
	SetInts(ints);

	return success;
}

/*
--------------------------------------------------------------------------------
SignalSem - senaliza un semaforo

Despierta a la primera tarea de la cola o incrementa la cuenta si la cola
esta vacia.
--------------------------------------------------------------------------------
*/

void
SignalSem(Semaphore_t *sem)
{
	bool ints = SetInts(false);
	if ( !SignalQueue(&sem->queue) )
		sem->value++;
	SetInts(ints);
}

/*
--------------------------------------------------------------------------------
ValueSem - informa la cuenta de un semaforo
--------------------------------------------------------------------------------
*/

unsigned	
ValueSem(Semaphore_t *sem)
{
	return sem->value;
}

/*
--------------------------------------------------------------------------------
FlushSem - despierta todas las tareas que esperan en un semaforo

Las tareas completan su WaitSem() con el status que se pasa como argumento.
Deja la cuenta en cero.
--------------------------------------------------------------------------------
*/

void
FlushSem(Semaphore_t *sem, bool wait_ok)
{
	bool ints = SetInts(false);
	sem->value = 0;
	FlushQueue(&sem->queue, wait_ok);
	SetInts(ints);
}
