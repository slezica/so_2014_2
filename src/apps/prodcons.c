#include <kernel.h>

#define FULL			0xDB
#define EMPTY			0xB0
#define BUF_SIZE		40

#define TPROD			200
#define TMON			40

#define MSG_COL			21
#define MSG_LIN			17
#define MSG_FMT			"%s"

#define BUF_COL			21
#define BUF_LIN			10
#define BUF_FMT			"%s"

#define TIME_COL		21
#define TIME_LIN		8
#define TIME_FMT		"Segundos:   %u"

#define PRODSTAT_COL	21
#define PRODSTAT_LIN	12
#define PRODSTAT_FMT	"Productor:  %s"

#define CONSSTAT_COL	21
#define CONSSTAT_LIN	13
#define CONSSTAT_FMT	"Consumidor: %s"

#define MAIN_FG			LIGHTCYAN
#define BUF_FG			YELLOW
#define CLK_FG			LIGHTGREEN
#define MON_FG			LIGHTRED


#define forever while(true)

// TLS

typedef struct 
{
	unsigned seconds;
	bool end_consumer;
	char buffer[BUF_SIZE+1];
	char *end;
	char *head;
	char *tail;
	Semaphore_t *buf_used, *buf_free;
	Task_t *prod, *cons, *clk, *mon;
} 
data;

#define seconds			TLS(data)->seconds
#define end_consumer	TLS(data)->end_consumer
#define buffer			TLS(data)->buffer
#define end 			TLS(data)->end
#define head 			TLS(data)->head
#define tail 			TLS(data)->tail
#define buf_used 		TLS(data)->buf_used
#define buf_free 		TLS(data)->buf_free
#define prod 			TLS(data)->prod
#define cons 			TLS(data)->cons
#define clk 			TLS(data)->clk
#define mon 			TLS(data)->mon

/* funciones de entrada-salida */

static int 
mprint(int fg, int x, int y, char *format, ...)
{
	int n;
	va_list args;

	Atomic();
	mt_cons_gotoxy(x, y);
	mt_cons_setattr(fg, BLACK);
	va_start(args, format);
	n = vprintk(format, args);
	va_end(args);
	mt_cons_clreol();
	Unatomic();
	return n;
}

static void
put_buffer(void)
{
	*tail++ = FULL;
	if ( tail == end )
		tail = buffer;
	mprint(BUF_FG, BUF_COL, BUF_LIN, BUF_FMT, buffer);
}

static void
get_buffer(void)
{
	*head++ = EMPTY;
	if ( head == end )
		head = buffer;
	mprint(BUF_FG, BUF_COL, BUF_LIN, BUF_FMT, buffer);
}

/* funciones auxiliares */

static const char *
task_state(Task_t *task)
{
	TaskInfo_t info;
	GetInfo(task, &info);
	return statename(info.state);
}

/* tareas */

static int
clock(void *arg)
{
	forever
	{
		mprint(CLK_FG, TIME_COL, TIME_LIN, TIME_FMT, seconds);
		Delay(1000);
		++seconds;
	}
	return 0;
}

static int
producer(void *arg)
{
	forever
	{
		WaitSem(buf_free);
		put_buffer();
		SignalSem(buf_used);
		Delay(TPROD);
	}
	return 0;
}

static int
consumer(void *arg)
{
	unsigned char c;

	forever
	{
		if ( (c = getch()) == 'S' || c == 's' )
			break;
		WaitSem(buf_used);
		get_buffer();
		SignalSem(buf_free);
	}

	end_consumer = true;
	return 0;
}

static int
monitor(void *args)
{
	forever
	{
		mprint(MON_FG, PRODSTAT_COL, PRODSTAT_LIN, PRODSTAT_FMT, task_state(prod));
		mprint(MON_FG, CONSSTAT_COL, CONSSTAT_LIN, CONSSTAT_FMT, task_state(cons));
		Delay(TMON);
	}
	return 0;
}

int
prodcons_main(int argc, char **argv)
{
	bool cursor = mt_cons_cursor(false);
	mt_cons_clear();

	TLS = Malloc(sizeof(data));

	end = buffer + BUF_SIZE;
	head = buffer;
	tail = buffer;
	memset(buffer, EMPTY, BUF_SIZE);

	buf_free = CreateSem("fee space", BUF_SIZE);
	buf_used = CreateSem("used space", 0);

	Ready(prod = CreateTask(producer, 0, NULL, "producer", DEFAULT_PRIO));
	Ready(cons = CreateTask(consumer, 0, NULL, "consumer", DEFAULT_PRIO));
	Ready(clk = CreateTask(clock, 0, NULL, "clock", DEFAULT_PRIO));
	Ready(mon = CreateTask(monitor, 0, NULL, "monitor", DEFAULT_PRIO + 1));

	mprint(MAIN_FG, MSG_COL, MSG_LIN, MSG_FMT, "Oprima S para salir\n");
	mprint(MAIN_FG, MSG_COL, MSG_LIN+1, MSG_FMT, "Cualquier otra tecla para activar el consumidor");

	while ( !end_consumer )
		Yield();

	DeleteTask(prod, 0);
	DeleteTask(clk, 0);
	DeleteTask(mon, 0);
	
	DeleteSem(buf_free);
	DeleteSem(buf_used);

	Free(TLS);

	mt_cons_cursor(cursor);
	mt_cons_clear();
	return 0;
}
