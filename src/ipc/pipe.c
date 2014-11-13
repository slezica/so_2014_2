#include <kernel.h>

struct Pipe_t
{
	char *			name;
	Monitor_t *		monitor;
	Condition_t *	cond_get;
	Condition_t *	cond_put;
	unsigned		size;
	unsigned		avail;
	char *			buf;
	char *			head;
	char *			tail;
	char *			end;
};

/*
--------------------------------------------------------------------------------
CreatePipe, DeletePipe - creacion y destruccion de pipes.

El parametro size determina el tamano del buffer interno. En principio puede
usarse cualquier valor; cuanto mas grande sea el buffer, mayor sera el
desacoplamiento entre las tarea que escriben y las que leen en el pipe.
--------------------------------------------------------------------------------
*/

Pipe_t *
CreatePipe(const char *name, unsigned size)
{
	char buf[200];
	Pipe_t *p = Malloc(sizeof(Pipe_t));

	p->head = p->tail = p->buf = Malloc(p->size = size);
	p->end = p->buf + size;
	p->monitor = CreateMonitor(name);
	p->name = GetName(p->monitor);
	sprintf(buf, "get %s", name);
	p->cond_get = CreateCondition(buf, p->monitor);
	sprintf(buf, "put %s", name);
	p->cond_put = CreateCondition(buf, p->monitor);

	return p;
}

void
DeletePipe(Pipe_t *p)
{
	DeleteCondition(p->cond_get);
	DeleteCondition(p->cond_put);
	DeleteMonitor(p->monitor);
	Free(p->buf);
	Free(p);
}

/*
--------------------------------------------------------------------------------
GetPipe, GetPipeCond, GetPipeTimed - lectura de un pipe.

GetPipe intenta leer size bytes de un pipe, bloqueandose si el pipe está vacío.
Retorna cuando puede leer algo, aunque sea una cantidad menor.
GetPipeTimed se comporta igual, pero puede salir prematuramente por timeout.
GetPipeCond lee los bytes que puede y retorna inmediatamente.

Estas funciones retornan la cantidad de bytes leidos.
--------------------------------------------------------------------------------
*/

unsigned
GetPipe(Pipe_t *p, void *data, unsigned size)
{
	return GetPipeTimed(p, data, size, FOREVER);
}

unsigned
GetPipeCond(Pipe_t *p, void *data, unsigned size)
{
	return GetPipeTimed(p, data, size, 0);
}

unsigned
GetPipeTimed(Pipe_t *p, void *data, unsigned size, unsigned msecs)
{
	unsigned i, nbytes;
	char *d;

	if ( !size || !EnterMonitor(p->monitor) )
		return 0;

	// Si hay un timeout finito, calcular deadline.
	Time_t deadline = (msecs && msecs != FOREVER) ? Time() + msecs : 0;

	// Bloquearse si el pipe está vacío
	while ( !p->avail ) 
	{
		// Desistir si es condicional, si no esperar
		if ( !msecs || !WaitConditionTimed(p->cond_get, msecs) )
		{
			LeaveMonitor(p->monitor);
			return 0;
		}
		// Si hay que seguir esperando con deadline, recalcular timeout
		if ( deadline && !p->avail )
		{
			Time_t now = Time();
			msecs = now < deadline ? deadline - now : 0;
		}
	}

	// Leer lo que se pueda
	for ( nbytes = min(size, p->avail), d = data, i = 0 ; i < nbytes ; i++ )
	{
		*d++ = *p->head++;
		if ( p->head == p->end )
			p->head = p->buf;
	}

	// Despertar eventuales escritores bloqueados
	if ( p->avail == p->size )
		BroadcastCondition(p->cond_put);
	p->avail -= nbytes;

	// Retornar cantidad de bytes leídos
	LeaveMonitor(p->monitor);
	return nbytes;
}

/*
--------------------------------------------------------------------------------
PutPipe, PutPipeCond, PutPipeTimed - escritura en un pipe.

PutPipe intenta escribir size bytes en el pipe, bloqueandose si el pipe está lleno.
Retorna cuando puede escribir algo, aunque sea una cantidad menor.
PutPipeTimed puede salir por timeout.
PutPipeCond escribe los bytes que puede y retorna inmediatamente.

Estas funciones retornan la cantidad de bytes escritos.
--------------------------------------------------------------------------------
*/

unsigned
PutPipe(Pipe_t *p, void *data, unsigned size)
{
	return PutPipeTimed(p, data, size, FOREVER);
}

unsigned
PutPipeCond(Pipe_t *p, void *data, unsigned size)
{
	return PutPipeTimed(p, data, size, 0);
}

unsigned
PutPipeTimed(Pipe_t *p, void *data, unsigned size, unsigned msecs)
{
	unsigned i, nbytes;
	char *d;

	if ( !size || !EnterMonitor(p->monitor) )
		return 0;

	// Si hay un timeout finito, calcular deadline.
	Time_t deadline = (msecs && msecs != FOREVER) ? Time() + msecs : 0;

	// Bloquearse si el pipe está lleno
	while ( p->avail == p->size ) 
	{
		// Desistir si es condicional, si no esperar
		if ( !msecs || !WaitConditionTimed(p->cond_put, msecs) )
		{
			LeaveMonitor(p->monitor);
			return 0;
		}
		// Si hay que seguir esperando con deadline, recalcular timeout
		if ( deadline && p->avail == p->size )
		{
			Time_t now = Time();
			msecs = now < deadline ? deadline - now : 0;
		}
	}

	// Escribir lo que se pueda
	for ( nbytes = min(size, p->size - p->avail), d = data, i = 0 ; i < nbytes ; i++ )
	{
		*p->tail++ = *d++;
		if ( p->tail == p->end )
			p->tail = p->buf;
	}

	// Despertar eventuales lectores bloqueados
	if ( !p->avail )
		BroadcastCondition(p->cond_get);
	p->avail += nbytes;

	// Retornar cantidad de bytes escritos
	LeaveMonitor(p->monitor);
	return nbytes;
}

/*
--------------------------------------------------------------------------------
AvailPipe - indica la cantidad de bytes almacenada en el pipe.
--------------------------------------------------------------------------------
*/

unsigned
AvailPipe(Pipe_t *p)
{
	return p->avail;
}
