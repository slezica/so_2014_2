#include <kernel.h>

int 
getch(void)
{
	unsigned char c;
	return mt_kbd_getch(&c) ? c : EOF;
}

int 
getch_cond(void)
{
	unsigned char c;
	return mt_kbd_getch_cond(&c) ? c : EOF;
}

int 
getch_timed(unsigned timeout)
{
	unsigned char c;
	return mt_kbd_getch_timed(&c, timeout) ? c : EOF;
}
