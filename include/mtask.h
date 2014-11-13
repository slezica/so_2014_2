#ifndef MTASK_H_INCLUDED
#define MTASK_H_INCLUDED

#include <lib.h>

#define MIN_PRIO		0
#define DEFAULT_PRIO	100
#define MAX_PRIO		-1U
#define FOREVER			-1U

typedef unsigned long long Time_t;
typedef struct Task_t Task_t;
typedef struct TaskQueue_t TaskQueue_t;

typedef int (*TaskFunc_t)(void *arg);
typedef void (*SaveRestore_t)(void);
typedef void (*Cleanup_t)(void);

typedef enum 
{ 
	TaskSuspended, 
	TaskReady, 
	TaskCurrent, 
	TaskDelaying, 
	TaskWaiting, 
	TaskSending, 
	TaskReceiving, 
	TaskJoining,
	TaskZombie,
	TaskTerminated 
} 
TaskState_t;

typedef struct
{
	Task_t *		task;
	unsigned		consnum;
	unsigned 		priority;
	TaskState_t		state;
	void *			waiting;
	bool 			is_timeout;
	unsigned 		timeout;
	bool			protected;
}
TaskInfo_t;

/* API principal */

Task_t *		CreateTask(TaskFunc_t func, unsigned stacksize, void *arg, const char *name, unsigned priority);
bool			DeleteTask(Task_t *task, int status);
bool			Protect(Task_t *task);
bool			Attach(Task_t *task);
bool			Detach(Task_t *task);
bool			SetPriority(Task_t *task, unsigned priority);
bool			SetConsole(Task_t *task, unsigned consnum);
bool			SetSaveRestore(Task_t *task, SaveRestore_t save, SaveRestore_t restore);
bool			SetCleanup(Task_t *task, Cleanup_t cleanup);
void			GetInfo(Task_t *task, TaskInfo_t *info);
TaskInfo_t *	GetTasks(unsigned *ntasks);
bool			Ready(Task_t *task);
bool			Suspend(Task_t *task);

Task_t *		CurrentTask(void);
void			Pause(void);
void			Yield(void);
void			Delay(unsigned msecs);
void			UDelay(unsigned usecs);
bool			Join(Task_t *task, int *status);
bool			JoinCond(Task_t *task, int *status);
bool			JoinTimed(Task_t *task, int *status, unsigned msecs);
void			Exit(int status);
void			Panic(const char *format, ...);
void			Atomic(void);
void			Unatomic(void);
bool	 		SetInts(bool enabled);

extern void *	TLS;
#define			TLS(type) ((type *)TLS)

TaskQueue_t *	CreateQueue(const char *name);
void			DeleteQueue(TaskQueue_t *queue);
bool			WaitQueue(TaskQueue_t *queue);
bool			WaitQueueTimed(TaskQueue_t *queue, unsigned msecs);
Task_t *		SignalQueue(TaskQueue_t *queue);
void			FlushQueue(TaskQueue_t *queue, bool success);

bool			Send(Task_t *to, void *msg, unsigned size);
bool			SendCond(Task_t *to, void *msg, unsigned size);
bool			SendTimed(Task_t *to, void *msg, unsigned size, unsigned msecs);
bool			Receive(Task_t **from, void *msg, unsigned *size);
bool			ReceiveCond(Task_t **from, void *msg, unsigned *size);
bool			ReceiveTimed(Task_t **from, void *msg, unsigned *size, unsigned msecs);

char *			GetName(void *object);
Time_t			Time(void);
void *			Malloc(unsigned size);
char *			StrDup(const char *str);
void 			Free(void *mem);

/* Semáforos */

typedef struct Semaphore_t Semaphore_t;

Semaphore_t *	CreateSem(const char *name, unsigned value);
void 			DeleteSem(Semaphore_t *sem);
bool 			WaitSem(Semaphore_t *sem);
bool 			WaitSemCond(Semaphore_t *sem);
bool 			WaitSemTimed(Semaphore_t *sem, unsigned msecs);
void 			SignalSem(Semaphore_t *sem);
unsigned		ValueSem(Semaphore_t *sem);
void 			FlushSem(Semaphore_t *sem, bool wait_ok);

/* Mutexes */

typedef struct Mutex_t Mutex_t;

Mutex_t *		CreateMutex(const char *name);
void 			DeleteMutex(Mutex_t *mut);
bool			EnterMutex(Mutex_t *mut);
bool			EnterMutexCond(Mutex_t *mut);
bool			EnterMutexTimed(Mutex_t *mut, unsigned msecs);
void			LeaveMutex(Mutex_t *mut);

/* Monitores y variables de condición */

typedef struct Monitor_t Monitor_t;
typedef struct Condition_t Condition_t;

Monitor_t *		CreateMonitor(const char *name);
void 			DeleteMonitor(Monitor_t *mon);
bool			EnterMonitor(Monitor_t *mon);
bool			EnterMonitorCond(Monitor_t *mon);
bool			EnterMonitorTimed(Monitor_t *mon, unsigned msecs);
void			LeaveMonitor(Monitor_t *mon);

Condition_t *	CreateCondition(const char *name, Monitor_t *mon);
void			DeleteCondition(Condition_t *cond);
bool			WaitCondition(Condition_t *cond);
bool			WaitConditionTimed(Condition_t *cond, unsigned msecs);
bool			SignalCondition(Condition_t *cond);
void			BroadcastCondition(Condition_t *cond);

/* Pipes */

typedef struct Pipe_t Pipe_t;

Pipe_t *		CreatePipe(const char *name, unsigned size);
void			DeletePipe(Pipe_t *p);
unsigned		GetPipe(Pipe_t *p, void *data, unsigned size);
unsigned		GetPipeCond(Pipe_t *p, void *data, unsigned size);
unsigned		GetPipeTimed(Pipe_t *p, void *data, unsigned size, unsigned msecs);
unsigned		PutPipe(Pipe_t *p, void *data, unsigned size);
unsigned		PutPipeCond(Pipe_t *p, void *data, unsigned size);
unsigned		PutPipeTimed(Pipe_t *p, void *data, unsigned size, unsigned msecs);
unsigned		AvailPipe(Pipe_t *p);

/* Colas de mensajes */

typedef struct MsgQueue_t MsgQueue_t;

MsgQueue_t *	CreateMsgQueue(const char *name, unsigned msg_max, unsigned msg_size);
void			DeleteMsgQueue(MsgQueue_t *mq);
bool			GetMsgQueue(MsgQueue_t *mq, void *msg);
bool			GetMsgQueueCond(MsgQueue_t *mq, void *msg);
bool			GetMsgQueueTimed(MsgQueue_t *mq, void *msg, unsigned msecs);
bool			PutMsgQueue(MsgQueue_t *mq, void *msg);
bool			PutMsgQueueCond(MsgQueue_t *mq, void *msg);
bool			PutMsgQueueTimed(MsgQueue_t *mq, void *msg, unsigned msecs);
unsigned		AvailMsgQueue(MsgQueue_t *mq);

#endif
