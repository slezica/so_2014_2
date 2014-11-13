#include <kernel.h>

#define VIDMEM 0xB8000
#define NUMROWS 25
#define NUMCOLS 80
#define VIDSIZE (2 * NUMROWS * NUMCOLS)
#define TABSIZE 8

#define CRT_ADDR 0x3D4
#define CRT_DATA 0x3D5
#define CRT_CURSOR_START 0x0A
#define CRT_CURSOR_END 0x0B
#define CRT_CURSOR_HIGH 0x0E
#define CRT_CURSOR_LOW 0x0F

#define DEFATTR ((BLACK << 12) | (LIGHTGRAY << 8))

#define BS 0x08

typedef unsigned short row[NUMCOLS];

typedef struct
{
	row *vidmem;
	struct
	{
		unsigned cur_x;
		unsigned cur_y;
		unsigned cur_attr;
		bool cursor_on;
		bool raw;
		unsigned scrolls;
	}
	status;
}
console;

static console real_console = 
{
	.vidmem = (row *) VIDMEM,
	{
		.cur_attr = DEFATTR, 
		.cursor_on = true
	}
};

static console vcons[NVCONS];

static unsigned focus;
static unsigned current;
static console *cons = &real_console;

static void 
init_vcons(unsigned n)
{
	console *cons = &vcons[n];
	cons->vidmem = Malloc(VIDSIZE);
	memcpy(cons->vidmem, real_console.vidmem, VIDSIZE);
	memcpy(&cons->status, &real_console.status, sizeof cons->status);
}

static void 
show_real(void)
{
	outb(CRT_ADDR, CRT_CURSOR_END);
	outb(CRT_DATA, real_console.status.cursor_on ? 15 : 0);
}

static void
set_real(void)
{
	if (!real_console.status.cursor_on)
		return;
	unsigned off = real_console.status.cur_y * NUMCOLS + real_console.status.cur_x;
	outb(CRT_ADDR, CRT_CURSOR_HIGH);
	outb(CRT_DATA, off >> 8);
	outb(CRT_ADDR, CRT_CURSOR_LOW);
	outb(CRT_DATA, off);
}

static void
save(void)
{
	bool ints = SetInts(false);
	memcpy(vcons[focus].vidmem, real_console.vidmem, VIDSIZE);
	vcons[focus].status = real_console.status;	
	SetInts(ints);
}

static void
restore(void)
{
	memcpy(real_console.vidmem, vcons[focus].vidmem, VIDSIZE);
	real_console.status = vcons[focus].status;	
	show_real();
	set_real();
}

static inline void
set_cons(void)
{
	cons = current == focus ? &real_console : &vcons[current];
}

static inline void
show_cursor(void)
{
	if (cons == &real_console)
		show_real();
}

static inline void
set_cursor(void)
{
	if (cons == &real_console)
		set_real();
}

#define	vidmem		cons->vidmem
#define	cur_x		cons->status.cur_x
#define	cur_y		cons->status.cur_y
#define	cur_attr	cons->status.cur_attr
#define	cursor_on	cons->status.cursor_on
#define	raw			cons->status.raw
#define	scrolls		cons->status.scrolls

static void
scroll(void)
{
	int j;
	for (j = 1; j < NUMROWS; j++)
		memcpy(&vidmem[j - 1], &vidmem[j], sizeof(row));
	for (j = 0; j < NUMCOLS; j++)
		vidmem[NUMROWS - 1][j] = DEFATTR;
	scrolls++;
}

static void
put(unsigned char ch)
{
	if (cur_x >= NUMCOLS)
	{
		cur_x = 0;
		if (cur_y == NUMROWS - 1)
			scroll();
		else
			cur_y++;
	}
	vidmem[cur_y][cur_x++] = (ch & 0xFF) | cur_attr;
	set_cursor();
}

/* Interfaz pública */

void
mt_cons_init(void)
{
	unsigned i;

	// Inicializar línea de origen del cursor
	outb(CRT_ADDR, CRT_CURSOR_START);
	outb(CRT_DATA, 14);

	// Mostrar cursor
	mt_cons_cursor(true);

	// Inicializar las consolas virtuales
	for (i = 0; i < NVCONS; i++)
		init_vcons(i);
}

void
mt_cons_clear(void)
{
	bool ints = SetInts(false);
	unsigned short *p1 = &vidmem[0][0];
	unsigned short *p2 = &vidmem[NUMROWS][0];
	while (p1 < p2)
		*p1++ = DEFATTR;
	mt_cons_gotoxy(0, 0);
	SetInts(ints);
}

void
mt_cons_clreol(void)
{
	bool ints = SetInts(false);
	unsigned short *p1 = &vidmem[cur_y][cur_x];
	unsigned short *p2 = &vidmem[cur_y + 1][0];
	while (p1 < p2)
		*p1++ = DEFATTR;
	SetInts(ints);
}

void
mt_cons_clreom(void)
{
	bool ints = SetInts(false);
	unsigned short *p1 = &vidmem[cur_y][cur_x];
	unsigned short *p2 = &vidmem[NUMROWS][0];
	while (p1 < p2)
		*p1++ = DEFATTR;
	SetInts(ints);
}

unsigned
mt_cons_nrows(void)
{
	return NUMROWS;
}

unsigned
mt_cons_ncols(void)
{
	return NUMCOLS;
}

unsigned
mt_cons_nscrolls(void)
{
	return scrolls;
}

void
mt_cons_getxy(unsigned *x, unsigned *y)
{
	bool ints = SetInts(false);
	*x = cur_x;
	*y = cur_y;
	SetInts(ints);
}

void
mt_cons_gotoxy(unsigned x, unsigned y)
{
	bool ints = SetInts(false);
	if (y < NUMROWS && x < NUMCOLS)
	{
		cur_x = x;
		cur_y = y;
		set_cursor();
	}
	SetInts(ints);
}

void
mt_cons_setattr(unsigned fg, unsigned bg)
{
	cur_attr = ((fg & 0xF) << 8) | ((bg & 0xF) << 12);
}

void
mt_cons_getattr(unsigned *fg, unsigned *bg)
{
	bool ints = SetInts(false);
	*fg = (cur_attr >> 8) & 0xF;
	*bg = (cur_attr >> 12) & 0xF;
	SetInts(ints);
}

bool
mt_cons_cursor(bool on)
{
	bool ints = SetInts(false);
	bool prev = cursor_on;
	cursor_on = on;
	show_cursor();
	set_cursor();
	SetInts(ints);
	return prev;
}

void
mt_cons_putc(char ch)
{
	bool ints = SetInts(false);
	if (raw)
	{
		put(ch);
		SetInts(ints);
		return;
	}
	switch (ch)
	{
		case '\t':
			mt_cons_tab();
			break;

		case '\r':
			mt_cons_cr();
			break;

		case '\n':
			mt_cons_nl();
			break;

		case BS:
			mt_cons_bs();
			break;

		default:
			put(ch);
			break;
	}
	SetInts(ints);
}

void
mt_cons_puts(const char *str)
{
	bool ints = SetInts(false);
	while (*str)
		mt_cons_putc(*str++);
	SetInts(ints);
}

void
mt_cons_cr(void)
{
	bool ints = SetInts(false);
	cur_x = 0;
	set_cursor();
	SetInts(ints);
}

void
mt_cons_nl(void)
{
	bool ints = SetInts(false);
	if (cur_y == NUMROWS - 1)
		scroll();
	else
		cur_y++;
	set_cursor();
	SetInts(ints);
}

void
mt_cons_tab(void)
{
	bool ints = SetInts(false);
	unsigned nspace = TABSIZE - (cur_x % TABSIZE);
	while (nspace--)
		put(' ');
	SetInts(ints);
}

void
mt_cons_bs(void)
{
	bool ints = SetInts(false);
	if (cur_x)
		cur_x--;
	else if (cur_y)
	{
		cur_y--;
		cur_x = NUMCOLS - 1;
	}
	set_cursor();
	SetInts(ints);
}

bool
mt_cons_raw(bool on)
{
	bool ints = SetInts(false);
	bool prev = raw;
	raw = on;
	SetInts(ints);
	return prev;
}

// Se llama en modo atómico y no desde interrupciones, salvo Panic()
void
mt_cons_setfocus(unsigned consnum)
{
	save();
	focus = consnum;
	restore();
	set_cons();
}

// Se llama con interrupciones deshabilitadas
void
mt_cons_setcurrent(unsigned consnum)
{
	current = consnum;
	set_cons();
}

// Se llama con interrupciones deshabilitadas
unsigned
mt_cons_set0(void)
{
	unsigned prev = current;
	current = 0;
	set_cons();
	return prev;
}