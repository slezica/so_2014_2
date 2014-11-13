#include <kernel.h>

#define INPUTSIZE 32

static MsgQueue_t *events[NVCONS];
static MsgQueue_t *keys[NVCONS];
static MsgQueue_t *input_focus, *input_current, *key_focus, *key_current;
static unsigned focus, current;

void 
mt_input_init(void)
{
	unsigned i;
	char buf [50];

	for ( i = 0 ; i < NVCONS ; i++ )
	{
		sprintf(buf, "input events %u", i);
		events[i] = CreateMsgQueue(buf, INPUTSIZE, sizeof(input_event_t));
		sprintf(buf, "input keys %u", i);
		keys[i] = CreateMsgQueue(buf, INPUTSIZE, 1);
	}
	input_current = input_focus = events[0];
	key_current = key_focus = keys[0];
}

bool 
mt_input_put(input_event_t *ev)
{
	return PutMsgQueueCond(input_focus, ev);
}

bool 
mt_input_get(input_event_t *ev)
{
	return GetMsgQueue(input_current, ev);
}

bool 
mt_input_get_cond(input_event_t *ev)
{
	return GetMsgQueueCond(input_current, ev);
}

bool 
mt_input_get_timed(input_event_t *ev, unsigned timeout)
{
	return GetMsgQueueTimed(input_current, ev, timeout);
}

bool
mt_kbd_putch(unsigned char c)
{
	return PutMsgQueueCond(key_focus, &c);
}

bool
mt_kbd_puts(unsigned char *s, unsigned len)	
{
	Atomic();
	if ( INPUTSIZE - AvailMsgQueue(key_focus) < len )
	{
		Unatomic();
		return false;
	}
	while ( len-- )
		PutMsgQueueCond(key_focus, s++);
	Unatomic();
	return true;
}

bool 
mt_kbd_getch(unsigned char *c)
{
	return GetMsgQueue(key_current, c);
}

bool 
mt_kbd_getch_cond(unsigned char *c)
{
	return GetMsgQueueCond(key_current, c);
}

bool 
mt_kbd_getch_timed(unsigned char *c, unsigned timeout)
{
	return GetMsgQueueTimed(key_current, c, timeout);
}

void 
mt_input_setfocus(unsigned consnum)			// No se llama desde interrupciones
{
	Atomic();
	if ( consnum != focus )
	{
		focus = consnum;
		input_focus = events[focus];
		key_focus = keys[focus];
		mt_cons_setfocus(focus);
	}
	Unatomic();
}

void 
mt_input_setcurrent(unsigned consnum)		// Se llama con interrupciones deshabilitadas
{
	if ( consnum == current )
		return;
	current = consnum;
	input_current = events[current];
	key_current = keys[current];
	mt_cons_setcurrent(current);
}

