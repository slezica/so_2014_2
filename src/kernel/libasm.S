#include <const.h>

.equ BIT_TS, 8

.global mt_load_gdt
.global mt_load_idt
.global mt_context_switch
.global mt_sti
.global mt_cli
.global mt_flags
.global mt_finit
.global mt_fsave
.global mt_frstor
.global mt_stts
.global mt_clts
.global mt_hlt

.extern mt_curr_task
.extern mt_last_task

.text

/*
Cargar la GDT e inicializar todos los registros de segmento
void mt_load_gdt(const region_desc *gdt);
*/
mt_load_gdt: 
	movl 4(%esp), %eax
	lgdt (%eax)
	movw $MT_DS, %ax			/* cargar segmentos de datos */
	movw %ax, %ds
	movw %ax, %es
	movw %ax, %fs
	movw %ax, %gs
	movw %ax, %ss
	ljmp $MT_CS, $end_load_gdt	/* cargar segmento de código */
end_load_gdt: 
	ret

/*
void mt_load_idt(const region_desc *idt);
Cargar la IDT
*/
mt_load_idt: 
	movl 4(%esp), %eax
	lidt (%eax)
	ret

/*
void context_switch(void);
Cambio de contexto fuera de una interrupción.
Mantener el stack frame sincronizado con el manejador de interrupciones
en interrupts.S y con la estructura mt_regs_t en kernel.h.
Esto se ejecuta con interrupciones deshabilitadas.
*/
mt_context_switch: 

	/* Simular interrupción y guardar contexto */
	popl retaddr
	pushfl
	pushl %cs
	pushl retaddr
	pushal
	
	/* Cambiar stack */
	movl mt_last_task, %eax
	movl %esp, Task_t_ESP(%eax)
	movl mt_curr_task, %eax
	movl Task_t_ESP(%eax), %esp

	/* Recuperar contexto */
	popal
	iret

/*
void mt_sti(void)
Habilitar interrupciones
*/
mt_sti: 
	sti
	ret

/*
void mt_cli(void)
Deshabilitar interrupciones
*/
mt_cli: 
	cli
	ret

/*
unsigned mt_flags(void)
Lee la palabra de flags.
*/
mt_flags:
	pushfl
	popl %eax
	ret

/*
void mt_finit(void);
Resetear el coprocesador aritmético
*/
mt_finit: 
	finit
	ret

/*
void mt_fsave(void *buf)
Guardar el contexto del coprocesador aritmético
*/
mt_fsave: 
	movl 4(%esp), %eax
	fsave (%eax)
	ret

/*
void mt_frstor(void *buf)
Restaurar el contexto del coprocesador aritmético
*/
mt_frstor: 
	fwait
	movl 4(%esp), %eax
	frstor (%eax)
	ret

/* void mt_stts(void); */
mt_stts: 
	movl %cr0, %eax
	orl $BIT_TS, %eax
	movl %eax, %cr0
	ret

/* void mt_clts(void); */
mt_clts: 
	clts
	ret

/* void mt_hlt(void); */
mt_hlt: 
	hlt
	ret

.bss

retaddr: .space 4