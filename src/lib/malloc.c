// Adaptado del libro "El lenguaje de programación C" de Kernighan y Ritchie

#include <kernel.h>

typedef union header Header;

union header
{
	struct
	{
		Header *ptr;					/* next block if on free list */
		unsigned size;					/* size of this block */
	};
	double align;						/* forzar alineacion a 8 bytes */
};

static Header *freep;
static Header base;

// Inicialización del heap. Recibe como argumento el tamaño en bytes de la 
// memoria superior (por encima de 1MB).
void 
mt_setup_heap(unsigned himem_size)
{
	extern char _end;					// fin del segmento de datos, ver mtask.map
	unsigned heapaddr, heapsize;

	// Determinar dirección y tamaño del heap
	heapaddr = ((unsigned) &_end + sizeof(Header) - 1) / sizeof(Header);
	heapaddr *= sizeof(Header);
	heapsize = himem_size - (heapaddr - 0x100000);

	// Inicializar lista de bloques libres
	base.ptr = freep = &base;			// bloque centinela de tamaño 0, nunca se aloca
	Header *ap = (Header *) heapaddr;	// bloque libre inicial, contiene todo el heap
	ap->size = heapsize / sizeof(Header);
	free(ap + 1);
}

// Liberar un bloque de memoria
void
free(void *ap)
{
	Header *bp, *p;

	bp = (Header *) ap - 1;				/* point to block header */
	for ( p = freep; !(bp > p && bp < p->ptr); p = p->ptr )
		if ( p >= p->ptr && (bp > p || bp < p->ptr) )
			break;						/* freed block at start or end of arena */
	if ( bp + bp->size == p->ptr ) 		/* join to upper nbr */
	{
		bp->size += p->ptr->size;
		bp->ptr = p->ptr->ptr;
	}
	else
		bp->ptr = p->ptr;
	if ( p + p->size == bp ) 			/* join to lower nbr */
	{
		p->size += bp->size;
		p->ptr = bp->ptr;
	}
	else
		p->ptr = bp;
	freep = p;
}

// Alocar un bloque de memoria
void *
malloc(unsigned nbytes)
{
	Header *p, *prevp;
	unsigned nunits = (nbytes + sizeof(Header) - 1) / sizeof(Header) + 1;

	for ( prevp = freep, p = prevp->ptr ; ; prevp = p, p = p->ptr )
	{
		if ( p->size >= nunits )		/* big enough */
		{
			if ( p->size == nunits )	/* exactly */
				prevp->ptr = p->ptr;
			else						/* allocate tail end */
			{
				p->size -= nunits;
				p += p->size;
				p->size = nunits;
			}
			freep = prevp;
			return p + 1;
		}
		if ( p == freep )				/* wrapped around free list */
			return 0;					/* none left */
	}
}
