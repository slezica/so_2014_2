.global inb
.global outb
.global inw
.global outw
.global inl
.global outl

.text

/* unsigned inb(unsigned ioaddr); */
inb: 
	pushl %edx
	movw 8(%esp), %dx
	xorl %eax, %eax
	inb %dx, %al
	popl %edx
	ret

/* void outb(unsigned ioaddr, unsigned data); */
outb: 
	pushl %edx
	movw 8(%esp), %dx
	movl 12(%esp), %eax
	outb %al, %dx
	popl %edx
	ret

/* unsigned inw(unsigned ioaddr); */
inw: 
	pushl %edx
	movw 8(%esp), %dx
	xorl %eax, %eax
	inw %dx, %ax
	popl %edx
	ret

/* void outw(unsigned ioaddr, unsigned data); */
outw: 
	pushl %edx
	movw 8(%esp), %dx
	movl 12(%esp), %eax
	outw %ax, %dx
	popl %edx
	ret

/* unsigned inl(unsigned ioaddr); */
inl: 
	pushl %edx
	movw 8(%esp), %dx
	inl %dx, %eax
	popl %edx
	ret

/* void outl(unsigned ioaddr, unsigned data); */
outl: 
	pushl %edx
	movw 8(%esp), %dx
	movl 12(%esp), %eax
	outl %eax, %dx
	popl %edx
	ret


