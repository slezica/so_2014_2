#include <kernel.h>

static TaskQueue_t time_q;

/*
--------------------------------------------------------------------------------
mt_enqueue - pone una tarea a esperar en una cola de tareas

La cola está ordenada por prioridad, y entre tareas de la misma prioridad por
el tiempo que llevan esperando. La última tarea de una cola es la de mayor
prioridad, y si hay más de una con la misma prioridad, la que lleva mayor tiempo
esperando.
--------------------------------------------------------------------------------
*/

void 
mt_enqueue(Task_t *task, TaskQueue_t *queue)
{
	Task_t *ta;

	/* Buscar donde insertar */
	for ( ta = queue->head ; ta && task->priority > ta->priority ; ta = ta->next )
		;
	if ( ta )		/* insertar antes de ta */
	{
		if ( (task->prev = ta->prev) )
			ta->prev->next = task;
		else
			queue->head = task;
		ta->prev = task;
		task->next = ta;
	}
	else if ( (ta = queue->tail) )	/* insertar al final de la cola */
	{
		ta->next = queue->tail = task;
		task->prev = ta;
		task->next = NULL;
	}
	else						/* la cola esta vacia */
	{
		queue->head = queue->tail = task;
		task->next = task->prev = NULL;
	}
	task->queue = queue;
}

/*
--------------------------------------------------------------------------------
mt_dequeue - si una tarea esta en una cola, la desencola.
--------------------------------------------------------------------------------
*/

void 
mt_dequeue(Task_t *task)
{
	TaskQueue_t *queue;

	if ( !(queue = task->queue) )
		return;
	if ( task->prev )
		task->prev->next = task->next;
	else
		queue->head = task->next;
	if ( task->next )
		task->next->prev = task->prev;
	else
		queue->tail = task->prev;
	task->next = task->prev = NULL;
	task->queue = NULL;
}

/*
--------------------------------------------------------------------------------
mt_peeklast, mt_getlast - acceso a la última tarea de una cola

Corresponde a la tarea de mayor prioridad, y si hay más de una con esta prioridad,
a la que lleva más tiempo esperando. 
Mt_peeklast devuelve la tarea sin desencolarla, mt_getlast la desencola.
--------------------------------------------------------------------------------
*/

Task_t *
mt_peeklast(TaskQueue_t *queue)
{
	return queue->tail;
}

Task_t *
mt_getlast(TaskQueue_t *queue)
{
	Task_t *task;

	if ( !(task = queue->tail) )
		return NULL;
	if ( (queue->tail = task->prev) )
		queue->tail->next = NULL;
	else
		queue->head = NULL;
	task->prev = task->next = NULL;
	task->queue = NULL;
	return task;
}

/*
--------------------------------------------------------------------------------
mt_enqueue_time - pone una tarea en la cola de tiempo.

Las tareas estan ordenados por el tiempo de despertarse, en forma diferencial.
El campo ticks de cada tarea se usa para almacenar los ticks de tiempo real 
que transcurren entre el tiempo de despertarse de cada tarea y la 
anterior en la cola. La primera tarea a despertar es la primera de la cola, y
su campo de ticks en un momento determinado indica cuantos ticks le faltan para
despertarse. La interrupcion de tiempo real decrementa el campo de ticks de la
primera tarea de la cola. Si el campo llega a cero, despierta a esta tarea
y a todas las que le sigan que tambien tengan el campo de ticks en cero.
--------------------------------------------------------------------------------
*/

void 
mt_enqueue_time(Task_t *task, unsigned ticks)
{
	Task_t *ta;

	/* Buscar donde insertar */
	for ( ta = time_q.head ; ta && ticks > ta->ticks ;
			ticks -= ta->ticks, ta = ta->time_next )
		;
	if ( ta )		/* insertar antes de ta */
	{
		if ( (task->time_prev = ta->time_prev) )
			ta->time_prev->time_next = task;
		else
			time_q.head = task;
		ta->time_prev = task;
		task->time_next = ta;
		ta->ticks -= ticks;
	}
	else if ( (ta = time_q.tail) )	/* insertar al final de la cola */
	{
		ta->time_next = time_q.tail = task;
		task->time_prev = ta;
		task->time_next = NULL;
	}
	else						/* la cola esta vacia */
	{
		time_q.head = time_q.tail = task;
		task->time_next = task->time_prev = NULL;
	}
	task->ticks = ticks;
	task->in_time_q = true;
}

/*
--------------------------------------------------------------------------------
mt_dequeue_time - quita una tarea de la cola de tiempo si esta en ella.
--------------------------------------------------------------------------------
*/

void 
mt_dequeue_time(Task_t *task)
{
	if ( !task->in_time_q )
		return;
	if ( task->time_prev )
		task->time_prev->time_next = task->time_next;
	else
		time_q.head = task->time_next;
	if ( task->time_next )
	{
		task->time_next->ticks += task->ticks;
		task->time_next->time_prev = task->time_prev;
	}
	else
		time_q.tail = task->time_prev;
	task->time_next = task->time_prev = NULL;
	task->in_time_q = false;
}

/*
--------------------------------------------------------------------------------
mt_peekfirst_time, mt_getfirst_time - acceso a la primera tarea de la cola 
de tiempo.
Corresponde a la primera tarea a despertar. Mt_peekfirst_time devuelve la
tarea sin desencolarla, mt_getfirst_time la desencola.
--------------------------------------------------------------------------------
*/

Task_t *
mt_peekfirst_time(void)
{
	return time_q.head;
}

Task_t *
mt_getfirst_time(void)
{
	Task_t *task;

	if ( !(task = time_q.head) )
		return NULL;
	if ( (time_q.head = task->time_next) )
		time_q.head->time_prev = NULL;
	else
		time_q.tail = NULL;
	task->time_prev = task->time_next = NULL;
	task->in_time_q = false;
	return task;
}
