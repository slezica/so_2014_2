#include <kernel.h>

#define BS		0x08
#define ESC		0x1b
#define ERASEBACK "\x08 \x08"

static int 
getc(void)
{
	int c;

	while ( (c = getch()) == EOF )
		;
	if ( c != ESC )
		return c;
	while ( (c = getch()) == EOF )
		;
	if ( c != '[' )
		return c;
	while ( (c = getch()) == EOF )
		;
	return -c;
}

int
getline(char *buf, unsigned size)
{
	char *p = buf + strlen(buf), *end = buf + size - 1;
	int c;
	unsigned xi, yi, si;
	
	mt_cons_getxy(&xi, &yi);
	si = mt_cons_nscrolls();

	mt_cons_puts(buf);
	mt_cons_clreol();
	while (p < end)
		switch (c = getc())
		{
			case BACK:
			case FWD:
			case FIRST:
			case LAST:
				mt_cons_gotoxy(xi, yi - (mt_cons_nscrolls() - si));
				mt_cons_clreom();
				*p = 0;
				return c;

			case BS:
				if (p == buf)
					break;
				if (*--p == '\t')
				{
					mt_cons_gotoxy(xi, yi - (mt_cons_nscrolls() - si));
					mt_cons_clreom();
					*p = 0;
					mt_cons_puts(buf);
				}
				else
					mt_cons_puts(ERASEBACK);
				break;

			case '\r':
			case '\n':
				mt_cons_puts("\r\n");
				*p++ = '\n';
				*p = 0;
				return p - buf;

			case EOF:
				*p = 0;
				return p - buf;

			default:
				*p++ = c;
				mt_cons_putc(c);
				break;
		}

	mt_cons_puts("<EOB>\r\n");
	*p = 0;
	return p - buf;
}
