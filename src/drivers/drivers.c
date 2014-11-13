#include <kernel.h>

void
mt_init_drivers(void)
{
	mt_cons_init();
	mt_input_init();
	mt_ps2_init();
	mt_ide_init();
}
