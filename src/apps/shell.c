#include <kernel.h>
#include <apps.h>

#define BUFSIZE 200
#define NAMESIZE 30
#define NARGS 20
#define NHIST 10

typedef int (*main_func)(int argc, char *argv[]);

typedef struct
{
	main_func func;
	int nargs;
	char *args[NARGS+1];
	char buf[BUFSIZE];
}
execpars;

static struct cmdentry
{
	char *name;
	main_func func;
	char *params;
}
cmdtab[] =
{
	{	"setkb",		setkb_main, 		"[distrib]"			},
	{	"shell",		shell_main,			""					},
	{	"sfilo",		simple_phil_main,	""					},
	{	"filo",			phil_main,			""					},
	{	"xfilo",		extra_phil_main,	""					},
	{	"afilo",		atomic_phil_main,	""					},
	{	"camino",		camino_main,		""					},
	{	"camino_ns",	camino_ns_main,		"[cantidad]"		},
	{	"prodcons",		prodcons_main,		""					},
	{	"divz",			divz_main,			"dividendo divisor"	},
	{	"pelu",			peluqueria_main,	""					},
	{	"events",		events_main,		""					},
	{	"disk",			disk_main,			""					},
	{	"ts", 			ts_main,			"[consola...]"		},
	{	"kill",			kill_main,			"tarea [status]"	},
	{	"test",			test_main,			""					},
	{															}
};

static int
attached_app(void *arg)
{
	execpars *ep = arg;

	return ep->func(ep->nargs, ep->args);
}

static int
detached_app(void *arg)
{
	execpars ex;					// parámetros en nuestro stack

	memcpy(&ex, arg, sizeof ex);	// copiar parámetros
	Receive(NULL, NULL, NULL);		// sincronizar con el shell

	// corrección de punteros en ex.args
	int fixup = (void *)&ex - arg;
	unsigned i;
	for ( i = 0 ; i < ex.nargs ; i++ )
		ex.args[i] += fixup;
	return ex.func(ex.nargs, ex.args);
}

static inline int
next(int n)
{
	if ( ++n == NHIST )
		n = 0;
	return n;
}

static inline int
prev(int n)
{
	if ( n-- == 0 )
		n = NHIST - 1;
	return n;
}

int
shell_main(int argc, char **argv)
{
	execpars ex;
	char line[BUFSIZE];
	struct cmdentry *cp;
	unsigned fg, bg;
	TaskInfo_t info;
	char *hist[NHIST];
	int pos, hfirst, hcur, hlast;
	bool wait, found;
	
	for ( hcur = 0 ; hcur < NHIST ; hcur++ )
		hist[hcur] = Malloc(BUFSIZE);
	hfirst = hcur = hlast = -1;

	mt_cons_getattr(&fg, &bg);
	GetInfo(CurrentTask(), &info);
	while ( true )
	{
		// Leer línea de comando eventualmente usando la historia
		mt_cons_setattr(LIGHTGRAY, BLACK);
		mt_cons_cursor(true);
		cprintk(LIGHTCYAN, BLACK, "\rMT%u> ", info.consnum);
		mt_cons_clreom();
		hcur = -1;
		*line = 0;
		do
		{
			switch ( pos = getline(line, sizeof line) )
			{
				case FIRST:
					if ( (hcur = hfirst) != -1)
						strcpy(line, hist[hcur]);
					break;
				case LAST:
					if ( (hcur = hlast) != -1 )
						strcpy(line, hist[hcur]);
					break;
				case BACK:	
					if ( hcur == -1 )
						hcur = hlast;
					else
						if ( hcur != hfirst )
							hcur = prev(hcur);
					if ( hcur != -1 )
						strcpy(line, hist[hcur]);
					break;
				case FWD:
					if ( hcur != -1 )
					{
						if ( hcur == hlast )
						{
							hcur = -1;
							*line = 0;
						}
						else
						{
							hcur = next(hcur);
							strcpy(line, hist[hcur]);
						}
					}
					break;
			}
		}
		while ( pos < 0 );

		// Sacar espacios al final y detectar comando en background
		wait = true;
		while ( --pos >= 0 )
		{
			char c = line[pos];
			switch ( c )
			{
				case ' ':
				case '\t':
				case '\r':
				case '\n':
					line[pos] = 0;
					continue;
			}
			if ( c == '&' )
				wait = false;
			break;
		}

		// Separar en argumentos
		strcpy(ex.buf, line);
		if ( !wait )
			ex.buf[pos] = 0;		// quitamos el & final antes de separar
		ex.nargs = separate(ex.buf, ex.args, NARGS);
		if ( !ex.nargs )
			continue;
		ex.args[ex.nargs] = NULL;

		// Guardar línea en la historia si es distinta de la última
		if ( hlast == -1 )
		{
			hlast = hfirst = 0;
			strcpy(hist[hlast], line);
		}
		else if ( strcmp(hist[hlast], line) != 0 )
		{
			hlast = next(hlast);
			if ( hfirst == hlast )
				hfirst = next(hlast);
			strcpy(hist[hlast], line);
		}

		/* Comandos internos */
		if ( strcmp(ex.args[0], "help") == 0 )
		{
			printk("Comandos internos:\n");
			printk("\thelp\n");
			printk("\texit [status]\n");
			printk("\treboot\n");
			printk("Aplicaciones:\n");\
			for ( cp = cmdtab ; cp->name ; cp++ )
				printk("\t%s %s\n", cp->name, cp->params);
			continue;
		}

		if ( strcmp(ex.args[0], "exit") == 0 )
		{
			mt_cons_setattr(fg, bg);
			for ( hcur = 0 ; hcur < NHIST ; hcur++ )
				Free(hist[hcur]);
			return ex.nargs > 1 ? atoi(ex.args[1]) : 0;
		}

		if ( strcmp(ex.args[0], "reboot") == 0 )
		{
			*(short *) 0x472 = 0x1234;
			while ( true )
				outb(0x64, 0xFE);
		}

		/* Aplicaciones */
		found = false;
		for ( cp = cmdtab ; cp->name ; cp++ )
			if ( strcmp(ex.args[0], cp->name) == 0 )
			{
				found = true;
				ex.func = cp->func;
				if ( wait )						// correr app y esperarla
				{
					int status;

					Task_t *t = CreateTask(attached_app, MAIN_STKSIZE, &ex, ex.args[0], DEFAULT_PRIO);
					Attach(t);
					Ready(t);
					while ( !Join(t, &status) )
						;
					if ( status != 0 )
					{
						cprintk(LIGHTRED, BLACK, "\rStatus: %d\n", status);
						mt_cons_clreol();
					}
				}
				else							// correr app en background
				{
					Task_t *t = CreateTask(detached_app, MAIN_STKSIZE, &ex, ex.args[0], DEFAULT_PRIO);
					cprintk(LIGHTGREEN, BLACK, "\rTask: %x\n", t);
					mt_cons_clreol();
					Ready(t);
					Send(t, NULL, 0);			// esperar que copie los parámetros
				}
				break;
			}

		if ( !found )
			cprintk(LIGHTRED, BLACK, "Comando %s desconocido\n", ex.args[0]);
	}
}
