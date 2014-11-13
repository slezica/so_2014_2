// Código de teclado copiado mayormente del driver Minix, especialmente
// todo lo relativo al mapeado de teclas. Usamos los mapas de Minix.
// El código de mouse es propio.

// Nota del original de Minix (keyboard.c):

/* Keyboard driver for PC's and AT's.
 *
 * Changes: 
 *   Jul 13, 2004   processes can observe function keys  (Jorrit N. Herder)
 *   Jun 15, 2004   removed wreboot(), except panic dumps (Jorrit N. Herder)
 *   Feb 04, 1994   loadable keymaps  (Marcus Hampel)
 */

#include <kernel.h>

// Definiciones para el mapa de teclas (de Minix: keymap.h).

#define C(c)    ((c) & 0x1F)    /* Map to control code          */
#define A(c)    ((c) | 0x80)    /* Set eight bit (ALT)          */
#define CA(c)   A(C(c))         /* Control-Alt                  */
#define L(c)    ((c) | HASCAPS) /* Add "Caps Lock has effect" attribute */

#define EXT     0x0100          /* Normal function keys         */
#define CTRL    0x0200          /* Control key                  */
#define SHIFT   0x0400          /* Shift key                    */
#define ALT     0x0800          /* Alternate key                */
#define EXTKEY  0x1000          /* extended keycode             */
#define HASCAPS 0x8000          /* Caps Lock has effect         */

/* Scan code conversion. */
#define KEY_RELEASE     0200
#define ASCII_MASK      0177

/* Numeric keypad */
#define HOME    (0x01 + EXT)
#define END     (0x02 + EXT)
#define UP      (0x03 + EXT)
#define DOWN    (0x04 + EXT)
#define LEFT    (0x05 + EXT)
#define RIGHT   (0x06 + EXT)
#define PGUP    (0x07 + EXT)
#define PGDN    (0x08 + EXT)
#define MID     (0x09 + EXT)
#define NMIN    (0x0A + EXT)
#define PLUS    (0x0B + EXT)
#define INSRT   (0x0C + EXT)

/* Alt + Numeric keypad */
#define AHOME   (0x01 + ALT)
#define AEND    (0x02 + ALT)
#define AUP     (0x03 + ALT)
#define ADOWN   (0x04 + ALT)
#define ALEFT   (0x05 + ALT)
#define ARIGHT  (0x06 + ALT)
#define APGUP   (0x07 + ALT)
#define APGDN   (0x08 + ALT)
#define AMID    (0x09 + ALT)
#define ANMIN   (0x0A + ALT)
#define APLUS   (0x0B + ALT)
#define AINSRT  (0x0C + ALT)

/* Ctrl + Numeric keypad */
#define CHOME   (0x01 + CTRL)
#define CEND    (0x02 + CTRL)
#define CUP     (0x03 + CTRL)
#define CDOWN   (0x04 + CTRL)
#define CLEFT   (0x05 + CTRL)
#define CRIGHT  (0x06 + CTRL)
#define CPGUP   (0x07 + CTRL)
#define CPGDN   (0x08 + CTRL)
#define CMID    (0x09 + CTRL)
#define CNMIN   (0x0A + CTRL)
#define CPLUS   (0x0B + CTRL)
#define CINSRT  (0x0C + CTRL)

/* Lock keys */
#define CALOCK  (0x0D + EXT)    /* caps lock    */
#define NLOCK   (0x0E + EXT)    /* number lock  */
#define SLOCK   (0x0F + EXT)    /* scroll lock  */

/* Function keys */
#define F1      (0x10 + EXT)
#define F2      (0x11 + EXT)
#define F3      (0x12 + EXT)
#define F4      (0x13 + EXT)
#define F5      (0x14 + EXT)
#define F6      (0x15 + EXT)
#define F7      (0x16 + EXT)
#define F8      (0x17 + EXT)
#define F9      (0x18 + EXT)
#define F10     (0x19 + EXT)
#define F11     (0x1A + EXT)
#define F12     (0x1B + EXT)

/* Alt+Fn */
#define AF1     (0x10 + ALT)
#define AF2     (0x11 + ALT)
#define AF3     (0x12 + ALT)
#define AF4     (0x13 + ALT)
#define AF5     (0x14 + ALT)
#define AF6     (0x15 + ALT)
#define AF7     (0x16 + ALT)
#define AF8     (0x17 + ALT)
#define AF9     (0x18 + ALT)
#define AF10    (0x19 + ALT)
#define AF11    (0x1A + ALT)
#define AF12    (0x1B + ALT)

/* Ctrl+Fn */
#define CF1     (0x10 + CTRL)
#define CF2     (0x11 + CTRL)
#define CF3     (0x12 + CTRL)
#define CF4     (0x13 + CTRL)
#define CF5     (0x14 + CTRL)
#define CF6     (0x15 + CTRL)
#define CF7     (0x16 + CTRL)
#define CF8     (0x17 + CTRL)
#define CF9     (0x18 + CTRL)
#define CF10    (0x19 + CTRL)
#define CF11    (0x1A + CTRL)
#define CF12    (0x1B + CTRL)

/* Shift+Fn */
#define SF1     (0x10 + SHIFT)
#define SF2     (0x11 + SHIFT)
#define SF3     (0x12 + SHIFT)
#define SF4     (0x13 + SHIFT)
#define SF5     (0x14 + SHIFT)
#define SF6     (0x15 + SHIFT)
#define SF7     (0x16 + SHIFT)
#define SF8     (0x17 + SHIFT)
#define SF9     (0x18 + SHIFT)
#define SF10    (0x19 + SHIFT)
#define SF11    (0x1A + SHIFT)
#define SF12    (0x1B + SHIFT)

/* Alt+Shift+Fn */
#define ASF1    (0x10 + ALT + SHIFT)
#define ASF2    (0x11 + ALT + SHIFT)
#define ASF3    (0x12 + ALT + SHIFT)
#define ASF4    (0x13 + ALT + SHIFT)
#define ASF5    (0x14 + ALT + SHIFT)
#define ASF6    (0x15 + ALT + SHIFT)
#define ASF7    (0x16 + ALT + SHIFT)
#define ASF8    (0x17 + ALT + SHIFT)
#define ASF9    (0x18 + ALT + SHIFT)
#define ASF10   (0x19 + ALT + SHIFT)
#define ASF11   (0x1A + ALT + SHIFT)
#define ASF12   (0x1B + ALT + SHIFT)

#define MAP_COLS        6       /* Number of columns in keymap */
#define NR_SCAN_CODES   0x80    /* Number of scan codes (rows in keymap) */

// Mapas de teclas

#define keymap keymap_spanish
#include <keymaps/spanish.src>
#undef keymap

#define keymap keymap_us_std
#include <keymaps/us-std.src>
#undef keymap

static const char *names[] =
{
	"spanish",
	"us-std",
	NULL
};

static unsigned short *keymaps[] =
{
	keymap_spanish,
	keymap_us_std
};

static const char *kbd_name;
static unsigned short *keymap;

/* Miscellaneous. */
#define ESC_SCAN        0x01	/* ESC */
#define SLASH_SCAN      0x35	/* numeric / */
#define RSHIFT_SCAN     0x36	/* right SHIFT */
#define HOME_SCAN       0x47	/* HOME */
#define INS_SCAN        0x52	/* INS */
#define DEL_SCAN        0x53	/* DEL */

#define NONE			-1U		/* no key */
#define ESC				0x1B	/* ESC */

static bool esc;				/* escape scan code detected? */
static bool alt_l;				/* left alt key state */
static bool alt_r;				/* right alt key state */
static bool alt;				/* either alt key */
static bool ctrl_l;				/* left control key state */
static bool ctrl_r;				/* right control key state */
static bool ctrl;				/* either control key */
static bool shift_l;			/* left shift key state */
static bool shift_r;			/* right shift key state */
static bool shift;				/* either shift key */
static bool num_down;			/* num lock key depressed */
static bool caps_down;			/* caps lock key depressed */
static bool scroll_down;		/* scroll lock key depressed */
static unsigned locks;			/* per console lock keys state */

/* Lock key active bits.  Chosen to be equal to the keyboard LED bits. */
#define SCROLL_LOCK     0x01
#define NUM_LOCK        0x02
#define CAPS_LOCK       0x04

static char numpad_map[] = { 'H', 'Y', 'A', 'B', 'D', 'C', 'V', 'U', 'G', 'S', 'T', '@' };

// Definiciones del controlador de teclado
#define KBD				0x60
#define KBDCTL			0x64
#define KBDIBF			1
#define KBDOBF			2
#define KBDINT			1
#define KBDTRIES		4000
#define KBDLEDS			0xED

// Definiciones para el mouse
#define MSEINT			12
#define MSEENABLE		0xA8
#define MSEGETSTAT		0x20
#define MSESETSTAT		0x60
#define MSEENAIRQ		0x02
#define MSECMD			0xD4
#define MSERESET		0xFF
#define MSEDEFAULT		0xF6
#define MSEPOLLED		0xF5
#define MSESTREAM		0xF4
#define MSEQUERY		0xEB
#define MSESETRATE		0xF3
#define MSEGETID		0xF2

// Tareas de entrada de teclas y eventos de mouse
#define INPUTPRIO		MAX_PRIO	// para que funcionen como "bottom half"
#define INPUTSIZE		32

static MsgQueue_t *scan_mq, *mouse_mq;

// Interrupción de teclado
static void 
kbdint(unsigned irq)
{
	unsigned c = inb(KBD);
	PutMsgQueueCond(scan_mq, &c);
}

static bool 
kbd_send(unsigned data)
{
	unsigned tries;
    for ( tries = KBDTRIES ; tries && (inb(KBDCTL) & KBDOBF) ; tries-- )
		Yield();
	if ( tries )
	{
	    outb(KBD, data);
		return true;	
	}
	return false;
}

static bool 
kbd_send_ctl(unsigned data)
{
	unsigned tries;
    for ( tries = KBDTRIES ; tries && (inb(KBDCTL) & KBDOBF) ; tries-- )
		Yield();
	if ( tries )
	{
	    outb(KBDCTL, data);
		return true;	
	}
	return false;
}

static int
kbd_receive(void)
{
	unsigned tries;
    for ( tries = KBDTRIES ; tries && !(inb(KBDCTL) & KBDIBF) ; tries-- )
		Yield();
	if ( tries )
		return inb(KBD);
	return -1;
}

// Esta función debe llamarse solamente después de instalar el manejador de
// la interrupción de teclado.
static void
set_leds()
{
	mt_disable_irq(KBDINT);
	kbd_send(KBDLEDS);
	kbd_receive();
	kbd_send(locks);
	kbd_receive();
	mt_enable_irq(KBDINT);
}

static unsigned
map_key(unsigned scode)
{
	/* Map a scan code to an ASCII code. */

	bool caps;
	unsigned column;
	unsigned short *keyrow;

	if (scode == SLASH_SCAN && esc)
		return '/';				/* don't map numeric slash */

	keyrow = &keymap[scode * MAP_COLS];

	caps = shift;
	if ((locks & NUM_LOCK) && HOME_SCAN <= scode && scode <= DEL_SCAN)
		caps = !caps;
	if ((locks & CAPS_LOCK) && (keyrow[0] & HASCAPS))
		caps = !caps;

	if (alt)
	{
		column = 2;
		if (ctrl || alt_r)
			column = 3;			/* Ctrl + Alt == AltGr */
		if (caps)
			column = 4;
	}
	else
	{
		column = 0;
		if (caps)
			column = 1;
		if (ctrl)
			column = 5;
	}
	return keyrow[column] & ~HASCAPS;
}

static unsigned
make_break(unsigned scode)
{
	/* This routine can handle keyboards that interrupt only on key depression,
	 * as well as keyboards that interrupt on key depression and key release.
	 * For efficiency, the interrupt routine filters out most key releases.
	 */

	unsigned ch;
	bool make, escape;

	/* High-order bit set on key release. */
	make = (scode & KEY_RELEASE) == 0;	/* true if pressed */

	ch = map_key(scode &= ASCII_MASK);	/* map to ASCII */

	escape = esc;				/* Key is escaped?  (true if added since the XT) */
	esc = false;

	switch (ch)
	{
		case CTRL:				/* Left or right control key */
			*(escape ? &ctrl_r : &ctrl_l) = make;
			ctrl = ctrl_l | ctrl_r;
			break;
		case SHIFT:			/* Left or right shift key */
			*(scode == RSHIFT_SCAN ? &shift_r : &shift_l) = make;
			shift = shift_l | shift_r;
			break;
		case ALT:				/* Left or right alt key */
			*(escape ? &alt_r : &alt_l) = make;
			alt = alt_l | alt_r;
			break;
		case CALOCK:			/* Caps lock - toggle on 0 -> 1 transition */
			if (!caps_down && make)
			{
				locks ^= CAPS_LOCK;
				set_leds();
			}
			caps_down = make;
			break;
		case NLOCK:			/* Num lock */
			if (!num_down && make)
			{
				locks ^= NUM_LOCK;
				set_leds();
			}
			num_down = make;
			break;
		case SLOCK:			/* Scroll lock */
			if (!scroll_down && make)
			{
				locks ^= SCROLL_LOCK;
				set_leds();
			}
			scroll_down = make;
			break;
		case EXTKEY:			/* Escape keycode */
			esc = true;			/* Next key is escaped */
			return NONE;
		default:				/* A normal key */
			return make ? ch : NONE;
	}

	return NONE;
}

static void
process_scan(unsigned scode)
{
	unsigned ch;

	/* Perform make/break processing. */
	if ( (ch = make_break(scode)) == NONE )
		return;

	if ( ch == CA('[') )	// ALT-ESC: foco en consola 0
	{
		mt_input_setfocus(0);
		return;
	}

	if ( 1 <= ch && ch <= 0xFF )
		/* A normal character. */
		mt_kbd_putch(ch);
	else if ( HOME <= ch && ch <= INSRT )
	{
		/* An ASCII escape sequence generated by the numeric pad. */
		unsigned char s[3] = { ESC, '[' };
		s[2] = numpad_map[ch - HOME];
		mt_kbd_puts(s, 3);
	}
	else if ( AF1 <= ch && ch < AF1 + NVCONS - 1)
		mt_input_setfocus(ch - AF1 + 1);				// AltFn: cambio de foco
	else
	{
		// Aquí deberían procesarse otras teclas especiales de procesamiento inmediato
	}
}

static int
kbd_task(void *arg)
{
	unsigned char scode;
	
	static unsigned more, idx;
	static input_event_t event = { KBD_EVENT };

	while ( true )
	{
		GetMsgQueue(scan_mq, &scode);

		if ( scode == 0xE0 )		// secuencia de dos códigos
		{
			event.kbd.scan_codes[0] = 0xE0;
			more = 1;
			idx = 1;
		}
		else if ( scode == 0xE1 )	// secuencia de tres códigos
		{
			event.kbd.scan_codes[0] = 0xE1;
			more = 2;
			idx = 1;
		}
		else if ( more )
		{
			event.kbd.scan_codes[idx++] = scode;
			if ( !--more )
			{
				mt_input_put(&event);
				process_scan(event.kbd.scan_codes[0]);
				process_scan(event.kbd.scan_codes[1]);
				if ( idx == 3 )
					process_scan(event.kbd.scan_codes[2]);
			}
		}
		else
		{
			event.kbd.scan_codes[0] = scode;
			mt_input_put(&event);
			process_scan(scode);
		}
	}
	return 0;
}

// Ejecutar antes de habilitar interrupciones de teclado
static void
init_mouse(void)
{
	// Habilitar PS2 auxiliar
	kbd_send_ctl(MSEENABLE);

	// Habilitar generación de IRQ12 leyendo y modificando
	// el byte de status
	unsigned status;
	kbd_send_ctl(MSEGETSTAT);
	status = kbd_receive();			// suponemos que no va a fracasar
	status |= MSEENAIRQ;			// habilitar la generación de IRQ12
	kbd_send_ctl(MSESETSTAT);
	kbd_send(status);

	// Poner el mouse en modo stream
	kbd_send_ctl(MSECMD);
	kbd_send(MSESTREAM);
	kbd_receive();
}

// Interrupción de mouse.
static void 
mouseint(unsigned irq)
{
	unsigned c = inb(KBD);
	PutMsgQueueCond(mouse_mq, &c);
}

static int
mouse_task(void *arg)
{
	static input_event_t event = { MOUSE_EVENT };

	while ( true )
	{
		// Leer 3 bytes, el primero tiene un bit que siempre debe estar en 1
		do
			GetMsgQueue(mouse_mq, &event.mouse.header);
		while ( !event.mouse.always_1 );
		GetMsgQueue(mouse_mq, &event.mouse.x);
		GetMsgQueue(mouse_mq, &event.mouse.y);
		mt_input_put(&event);
	}
	return 0;
}

// Interfaz

void
mt_ps2_init(void)
{
	// Establecer la distribución de teclado inicial.
	keymap = keymaps[0];
	kbd_name = names[0];

	// Crear colas de mensajes.
	scan_mq = CreateMsgQueue("scan codes", INPUTSIZE, 1);
	mouse_mq = CreateMsgQueue("mouse bytes", INPUTSIZE, 1);

	// Habilitar e inicializar el mouse
	init_mouse();

	// Correr tareas auxiliares
	Task_t *t = CreateTask(kbd_task, 0, NULL, "keyboard", INPUTPRIO);
	Protect(t);
	Ready(t);
	t = CreateTask(mouse_task, 0, NULL, "mouse", INPUTPRIO);
	Protect(t);
	Ready(t);

	// Instalar manejadores de interrupción y habilitar interrupciones
	mt_set_int_handler(KBDINT, kbdint);
	mt_enable_irq(KBDINT);
	mt_set_int_handler(MSEINT, mouseint);
	mt_enable_irq(MSEINT);

	// Sincronizar el estado de los LEDs del teclado
	set_leds();	
}

const char *
mt_ps2_getlayout(void)
{
	return kbd_name;
}

bool
mt_ps2_setlayout(const char *name)
{
	unsigned n;
	const char *p;

	for ( n = 0 ; (p = names[n]) ; n++ )
		if ( strcmp(name, p) == 0 )
		{
			kbd_name = p;
			keymap = keymaps[n];
			return true;
		}
	return false;
}

const char **
mt_ps2_layouts(void)
{
	return names;
}

