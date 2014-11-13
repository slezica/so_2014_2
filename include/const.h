// Segmentos de MTask (inicializados en gdt_idt.c)
#define MT_CS 0x08				// segmento de código
#define MT_DS 0x10				// segmento de datos

// Offset de esp en la estructura Task_t (definida en kernel.h)
#define Task_t_ESP 24

// Stubs de interrupción (definidos en interrupts.S)
// Excepciones 0-31, interrupciones de HW 32-47
#define INT_STUB_SIZE 16		// tamaño de cada stub
#define NUM_INTS 48				// total de stubs
#define NUM_EXCEPT 32			// cantidad de excepciones

// Tamaños de stack
#define MAIN_STKSIZE 0x4000		// tarea inicial y shells iniciales
#define INT_STKSIZE 0x4000		// interrupciones
#define MIN_STACK 0x1000		// mínimo para una tarea 