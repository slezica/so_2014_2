.extern mt_curr_task
.extern mt_select_task
.extern mt_int_handler
.extern mt_int_level
.extern mt_exit_point

.global mt_int_stubs

#include <const.h>

.text

/*
Stub para excepciones que empujan un código de error al stack
*/

#define int_error(n)	 \
.align INT_STUB_SIZE	;\
	pushl $n			;\
	jmp common_handler	

/*
Stub para las demás excepciones e interrupciones de hardware, 
empuja un código de error dummy 
*/

#define int_noerror(n)	 \
.align INT_STUB_SIZE	;\
	pushl $0			;\
	pushl $n			;\
	jmp common_handler	

/*
Stubs de interrupción.

typedef char int_stub[INT_STUB_SIZE];
extern int_stub mt_int_stubs[NUM_INTS];
*/

.align INT_STUB_SIZE
mt_int_stubs:

/* Excepciones */

int_noerror (0)
int_noerror (1)
int_noerror (2)
int_noerror (3)
int_noerror (4)
int_noerror (5)
int_noerror (6)
int_noerror (7)
int_error   (8)
int_noerror (9)
int_error  (10)
int_error  (11)
int_error  (12)
int_error  (13)
int_error  (14)
int_noerror(15)
int_noerror(16)
int_error  (17)
int_noerror(18)
int_noerror(19)
int_noerror(20)
int_noerror(21)
int_noerror(22)
int_noerror(23)
int_noerror(24)
int_noerror(25)
int_noerror(26)
int_noerror(27)
int_noerror(28)
int_noerror(29)
int_noerror(30)
int_noerror(31)

/* Interrupciones de hardware */

int_noerror(32)
int_noerror(33)
int_noerror(34)
int_noerror(35)
int_noerror(36)
int_noerror(37)
int_noerror(38)
int_noerror(39)
int_noerror(40)
int_noerror(41)
int_noerror(42)
int_noerror(43)
int_noerror(44)
int_noerror(45)
int_noerror(46)
int_noerror(47)

/*
Código común para todos los manejadores 
*/

common_handler: 

	/*
	Guardamos parámetros en variables estáticas.
	Están protegidas porque están deshabilitadas las interrupciones
	(todas las entradas de la IDT son interrupt gates).
	*/
	popl int_number					/* número de interrupción */
	popl except_error				/* código de error */

	/*
	Empujar al stack el contexto de registros. Esto tiene que estar
	sincronizado con la estructura mt_regs_t en kernel.h y con la función
	mt_context_switch() en libasm.S.
	*/
	pushal
	movl %esp, regs_ptr				/* puntero a los registros */

	/*
	Si se trata de una interrupción de primer nivel, cambiamos a un stack
	interno. Para interrupciones anidadas mantenemos el mismo stack.
	*/
	incl mt_int_level
	cmpl $1, mt_int_level
	jne stack_ok1
	movl mt_curr_task, %eax
	movl %esp, Task_t_ESP(%eax)		/* guardar stack actual */
	movl $int_stack_end, %esp		/* cambiar a stack interno */

stack_ok1: 

	/*
	Llamamos al manejador genérico en C, pasándole como argumentos número
	de interrupción, código de error (que solamente será distinto de cero para
	algunas excepciones) y puntero a la estructura de registros. El manejador
	retorna con interrupciones deshabilitadas.
	*/
	pushl regs_ptr
	pushl except_error
	pushl int_number
	call mt_int_handler				/* mt_int_handler(int_number, except_error, regs) */
	addl $12, %esp

	/*
	Si estamos retornando de una interrupción de primer nivel,
	llamamos a mt_select_task() para que eventualmente cambie la
	tarea actual, y cambiamos al stack de esa tarea. Si se trata
	de una interrupción anidada seguimos con el mismo stack.
	*/
	decl mt_int_level
	jnz stack_ok2
	call mt_select_task				/* puede cambiar la tarea actual */
	movl mt_curr_task, %eax
	movl Task_t_ESP(%eax), %esp		/* cambiar al stack de la tarea actual */

stack_ok2: 

	/* Recuperar contexto */
	popal
	iret

.bss

except_error: .space 4
int_number: .space 4
regs_ptr: .space 4

.align 4
int_stack: .space INT_STKSIZE
int_stack_end: 
