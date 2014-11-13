#include <const.h>

#define MULTIBOOT_PAGE_ALIGN	1<<0
#define MULTIBOOT_MEMORY_INFO	1<<1
#define MULTIBOOT_HEADER_MAGIC	0x1BADB002
#define MULTIBOOT_HEADER_FLAGS	MULTIBOOT_PAGE_ALIGN | MULTIBOOT_MEMORY_INFO
#define MULTIBOOT_CHECKSUM		-(MULTIBOOT_HEADER_MAGIC + MULTIBOOT_HEADER_FLAGS)

.global _start
.extern mt_main                 /* en kernel.c */

.text

/*
Header multiboot, requerido por GRUB. Debe aparecer en los primeros 8 KB del
ejecutable alineado a 4 bytes.
*/

.long MULTIBOOT_HEADER_MAGIC
.long MULTIBOOT_HEADER_FLAGS
.long MULTIBOOT_CHECKSUM

/*
Punto de entrada del kernel.
Las interrupciones están deshabilitadas, el stack es indefinido. Los registros
de segmentos están cargados pero no es seguro que haya una GDT válida, por lo
tanto no pueden tocarse hasta no establecer una GDT propia. Lo primero que
hacemos es poner un stack para poder ejecutar código en C. El resto lo
inicializamos en C.
*/

_start: 
	movl $stack_end, %esp	/* establecer stack */
	pushl %ebx				/* puntero a estructura de informacion del bootloader */
	pushl %eax				/* numero magico del bootloader */
	call mt_main			/* seguimos en C */
	cli						/* mt_main no deberia retornar */
	hlt

.bss

.align 4
stack: .space MAIN_STKSIZE
stack_end: 

