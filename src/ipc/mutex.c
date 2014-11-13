#include <kernel.h>

struct Mutex_t
{
	TaskQueue_t		queue;
	unsigned		use_count;
	Task_t *		owner;
};

/*
--------------------------------------------------------------------------------
CreateMutex - aloca un mutex inicialmente libre
--------------------------------------------------------------------------------
*/

Mutex_t *
CreateMutex(const char *name)
{
	Mutex_t *mut = Malloc(sizeof(Mutex_t));

	mut->queue.name = StrDup(name);
	return mut;
}

/*
--------------------------------------------------------------------------------
DeleteMutex - da de baja un mutex
--------------------------------------------------------------------------------
*/

void 			
DeleteMutex(Mutex_t *mut)
{
	FlushQueue(&mut->queue, false);
	Free(GetName(mut));
	Free(mut);
}

/*
--------------------------------------------------------------------------------
EnterMutex, EnterMutexCond, EnterMutexTimed - ocupar un mutex.

El valor de retorno indica si la operacion fue exitosa, en cuyo caso la tarea
actual es dueña del mutex.
El mutex puede tomarse anidadamente, para liberarlo debe llamarse tantas
veces a LeaveMutex como las que se lo ocupo exitosamente.
--------------------------------------------------------------------------------
*/

bool			
EnterMutex(Mutex_t *mut)
{
	return EnterMutexTimed(mut, FOREVER);
}

bool			
EnterMutexCond(Mutex_t *mut)
{
	return EnterMutexTimed(mut, 0);
}

bool			
EnterMutexTimed(Mutex_t *mut, unsigned msecs)
{
	if ( mut->owner == mt_curr_task )
	{
		mut->use_count++;
		return true;
	}
	Atomic();
	if ( !mut->owner )
	{
		mut->owner = mt_curr_task;
		mut->use_count = 1;
		Unatomic();
		return true;
	}
	bool success = WaitQueueTimed(&mut->queue, msecs);
	Unatomic();
	return success;
}

/*
--------------------------------------------------------------------------------
LeaveMutex - libera un mutex

Para liberar un mutex, debe llamarse tantas veces como se lo ocupo. Produce
un error fatal si la tarea actual no es dueña del mutex.
--------------------------------------------------------------------------------
*/

void			
LeaveMutex(Mutex_t *mut)
{
	if ( mut->owner != mt_curr_task )
		Panic("LeaveMutex %s: la tarea no posee el mutex", GetName(mut));

	if ( !--mut->use_count )
	{
		Atomic();
		if ( (mut->owner = SignalQueue(&mut->queue)) )
			mut->use_count = 1;
		Unatomic();
	}
}

