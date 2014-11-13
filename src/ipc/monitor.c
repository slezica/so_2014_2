#include <kernel.h>

struct Monitor_t
{
	TaskQueue_t		queue;
	Task_t *		owner;
};

struct Condition_t
{
	TaskQueue_t		queue;
	Monitor_t *		monitor;
};

/*
--------------------------------------------------------------------------------
CreateMonitor - aloca un monitor inicialmente libre
--------------------------------------------------------------------------------
*/

Monitor_t *
CreateMonitor(const char *name)
{
	Monitor_t *mon = Malloc(sizeof(Monitor_t));

	mon->queue.name = StrDup(name);
	return mon;
}

/*
--------------------------------------------------------------------------------
DeleteMonitor - da de baja un monitor
--------------------------------------------------------------------------------
*/

void 			
DeleteMonitor(Monitor_t *mon)
{
	FlushQueue(&mon->queue, false);
	Free(GetName(mon));
	Free(mon);
}

/*
--------------------------------------------------------------------------------
EnterMonitor, EnterMonitorCond, EnterMonitorTimed - ocupar un monitor.

El valor de retorno indica si la operacion fue exitosa, en cuyo caso la
tarea es dueña del monitor.
--------------------------------------------------------------------------------
*/

bool			
EnterMonitor(Monitor_t *mon)
{
	return EnterMonitorTimed(mon, FOREVER);
}

bool			
EnterMonitorCond(Monitor_t *mon)
{
	return EnterMonitorTimed(mon, 0);
}

bool			
EnterMonitorTimed(Monitor_t *mon, unsigned msecs)
{
	if ( mon->owner == mt_curr_task )
		Panic("EnterMonitorTimed: monitor %s ya ocupado por esta tarea", GetName(mon));

	Atomic();
	if ( !mon->owner )
	{
		mon->owner = mt_curr_task;
		Unatomic();
		return true;
	}
	bool success = WaitQueueTimed(&mon->queue, msecs);
	Unatomic();

	return success;
}

/*
--------------------------------------------------------------------------------
LeaveMonitor - libera un monitor
--------------------------------------------------------------------------------
*/

void			
LeaveMonitor(Monitor_t *mon)
{
	if ( mon->owner != mt_curr_task )
		Panic("LeaveMonitor: la tarea no posee el monitor %s", GetName(mon));

	Atomic();
	mon->owner = SignalQueue(&mon->queue);
	Unatomic();
}

/*
--------------------------------------------------------------------------------
CreateCondition - crea una variable de condicion asociada a un monitor
--------------------------------------------------------------------------------
*/

Condition_t *		
CreateCondition(const char *name, Monitor_t *mon)
{
	Condition_t *cond = Malloc(sizeof(Condition_t));

	cond->queue.name = StrDup(name);
	cond->monitor = mon;
	return cond;
}

/*
--------------------------------------------------------------------------------
DeleteCondition - da de baja una variable de condicion
--------------------------------------------------------------------------------
*/

void				
DeleteCondition(Condition_t *cond)
{
	FlushQueue(&cond->queue, false);
	Free(GetName(cond));
	Free(cond);
}

/*
--------------------------------------------------------------------------------
WaitCondition, WaitConditionTimed - esperar una condicion

La tarea que espera en la variable de condicion debe estar dentro del monitor.
Estas funciones atomicamente dejan el monitor, esperan en la cola de tareas
de la condicion y vuelven a tomar el monitor para retornar el resultado de la
espera. 
--------------------------------------------------------------------------------
*/

bool				
WaitCondition(Condition_t *cond)
{
	return WaitConditionTimed(cond, FOREVER);
}

bool				
WaitConditionTimed(Condition_t *cond, unsigned msecs)
{
	bool success;
	Monitor_t *mon = cond->monitor;

	if ( mon->owner != mt_curr_task )
		Panic("WaitConditionTimed %s: la tarea no posee el monitor %s", GetName(cond), GetName(cond->monitor));

	Atomic();
	LeaveMonitor(mon);
	success = WaitQueueTimed(&cond->queue, msecs);
	while ( !EnterMonitor(mon) )	// Hay que volver a tomar el monitor si o si
		;
	Unatomic();

	return success;
}

/*
--------------------------------------------------------------------------------
SignalCondition - senalizar una condicion

La tarea que senaliza la variable de condicion debe estar dentro del monitor.
Esta funcion despierta una tarea de las que esten esperando en la cola de
tareas de la condicion, si hay alguna. La tarea despertada completa
exitosamente su WaitConditionTimed.
El valor de retorno indica si se ha despertado a una tarea.
--------------------------------------------------------------------------------
*/

bool				
SignalCondition(Condition_t *cond)
{
	if ( cond->monitor->owner != mt_curr_task )
		Panic("SignalCondition %s: la tarea no posee el monitor %s", GetName(cond), GetName(cond->monitor));

	return SignalQueue(&cond->queue) != NULL;
}

/*
--------------------------------------------------------------------------------
BroadcastCondition - senalizar en broadcast una condicion

La tarea que senaliza la variable de condicion debe estar dentro del monitor.
Esta funcion despierta a todas las tareas que esten esperando en la cola de
tareas de la condicion, los cuales completan exitosamente sus
WaitConditionTimed.
--------------------------------------------------------------------------------
*/

void				
BroadcastCondition(Condition_t *cond)
{
	if ( cond->monitor->owner != mt_curr_task )
		Panic("BroadcastCondition %s: la tarea no posee el monitor %s", GetName(cond), GetName(cond->monitor));

	FlushQueue(&cond->queue, true);
}
