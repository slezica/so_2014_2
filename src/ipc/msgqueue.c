#include <kernel.h>

struct MsgQueue_t
{
	char *			name;
	Semaphore_t *	sem_get;
	Semaphore_t *	sem_put;
	unsigned		msg_size;
	char *			buf;
	char *			head;
	char *			tail;
	char *			end;
};

/*
--------------------------------------------------------------------------------
CreateMsgQueue, DeleteMsgQueue - creacion y destruccion de colas de mensajes.

El parametro msg_max indica la maxima cantidad de mensajes a almacenar, y
msg_size el tamano de cada uno. Los otros parametros determinan si se desea 
tener mutexes de lectura y escritura en la cola, para permitir la existencia de
varios consumidores y/o productores de mensajes.
--------------------------------------------------------------------------------
*/

MsgQueue_t *
CreateMsgQueue(const char *name, unsigned msg_max, unsigned msg_size)
{
	char buf[200];
	MsgQueue_t *mq;
	unsigned size = msg_max * msg_size;

	if ( size / msg_size != msg_max )	
		Panic("CreateMsgQueue %s: excede capacidad", name);

	mq = Malloc(sizeof(MsgQueue_t));
	mq->name = StrDup(name);
	mq->msg_size = msg_size;
	mq->head = mq->tail = mq->buf = Malloc(size);
	mq->end = mq->buf + size;
	sprintf(buf, "get %s", name);
	mq->sem_get = CreateSem(buf, 0);
	sprintf(buf, "put %s", name);
	mq->sem_put = CreateSem(buf, msg_max);

	return mq;
}

void
DeleteMsgQueue(MsgQueue_t *mq)
{
	DeleteSem(mq->sem_get);
	DeleteSem(mq->sem_put);
	Free(mq->buf);
	Free(mq->name);
	Free(mq);
}

/*
--------------------------------------------------------------------------------
GetMsgQueue, GetMsgQueueCond, GetMsgQueueTimed - lectura de un mensaje
--------------------------------------------------------------------------------
*/

bool
GetMsgQueue(MsgQueue_t *mq, void *msg)
{
	return GetMsgQueueTimed(mq, msg, FOREVER);
}

bool
GetMsgQueueCond(MsgQueue_t *mq, void *msg)
{
	return GetMsgQueueTimed(mq, msg, 0);
}

bool
GetMsgQueueTimed(MsgQueue_t *mq, void *msg, unsigned msecs)
{
	if ( !WaitSemTimed(mq->sem_get, msecs) )
		return false;

	bool ints = SetInts(false);
	memcpy(msg, mq->head, mq->msg_size);
	mq->head += mq->msg_size;
	if ( mq->head == mq->end )
		mq->head = mq->buf;
	SetInts(ints);

	SignalSem(mq->sem_put);

	return true;
}

/*
--------------------------------------------------------------------------------
PutMsgQueue, PutMsgQueueCond, PutMsgQueueTimed - escritura de un mensaje
--------------------------------------------------------------------------------
*/

bool
PutMsgQueue(MsgQueue_t *mq, void *msg)
{
	return PutMsgQueueTimed(mq, msg, FOREVER);
}

bool
PutMsgQueueCond(MsgQueue_t *mq, void *msg)
{
	return PutMsgQueueTimed(mq, msg, 0);
}

bool
PutMsgQueueTimed(MsgQueue_t *mq, void *msg, unsigned msecs)
{
	if ( !WaitSemTimed(mq->sem_put, msecs) )
		return false;

	bool ints = SetInts(false);
	memcpy(mq->tail, msg, mq->msg_size);
	mq->tail += mq->msg_size;
	if ( mq->tail == mq->end )
		mq->tail = mq->buf;
	SetInts(ints);

	SignalSem(mq->sem_get);

	return true;
}

/*
--------------------------------------------------------------------------------
AvailMsgQueue - indica la cantidad de mensajes almacenada en la cola
--------------------------------------------------------------------------------
*/

unsigned
AvailMsgQueue(MsgQueue_t *mq)
{
	return ValueSem(mq->sem_get);
}
