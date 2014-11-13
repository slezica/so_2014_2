#ifndef LIB_H_INCLUDED
#define LIB_H_INCLUDED

#include <stdarg.h>

#ifndef NULL
#define NULL 0
#endif

#ifndef max
#define max(a, b) ((a) > (b) ? (a) : (b))
#endif

#ifndef min
#define min(a, b) ((a) < (b) ? (a) : (b))
#endif

#define EOF -1

typedef enum { false, true } bool;

/* io.S */

unsigned inb(unsigned ioaddr);
void outb(unsigned ioaddr, unsigned data);
unsigned inw(unsigned ioaddr);
void outw(unsigned ioaddr, unsigned data);
unsigned inl(unsigned ioaddr);
void outl(unsigned ioaddr, unsigned data);

/* string.c */

char *strcpy(char *dest, const char *src);
char *strncpy(char *dest, const char *src, unsigned count);
char *strcat(char *dest, const char *src);
char *strncat(char *dest, const char *src, unsigned count);
int strcmp(const char *cs, const char *ct);
int strncmp(const char *cs, const char *ct, unsigned count);
char *strchr(const char *s, int c);
char *strrchr(const char *s, int c);
unsigned strlen(const char *s);
unsigned strnlen(const char *s, unsigned count);
void *memcpy(void *to, const void *from, unsigned n);
void *memmove(void *dest, const void *src, unsigned n);
void *memchr(const void *cs, int c, unsigned count);
void *memset(void *s, int c, unsigned count);

/* rand.c */

int rand(void);
void srand(unsigned seed);

/* getch.c */

int getch(void);
int getch_cond(void);
int getch_timed(unsigned timeout);

/* getline.c */

#define BACK	(-'A')
#define FWD 	(-'B')
#define FIRST 	(-'H')
#define LAST	(-'Y')

int getline(char *buf, unsigned size);

/* sprintf.c */

int vsprintf(char *buf, const char *fmt, va_list args);
int sprintf(char *buf, const char *fmt, ...);

/* printk.c */

int vprintk(const char *fmt, va_list args);
int printk(const char *fmt, ...);
int print0(const char *fmt, ...);
void cprintk(unsigned fg, unsigned bg, const char *fmt, ...);

/* malloc.c */

void *malloc(unsigned nbytes);
void free(void *ap);

/* split.c */

const char *setfs(const char *fs);
unsigned split(char *s, char *field[], unsigned nfields);
unsigned separate(char *s, char *field[], unsigned nfields);

/* atoi.c **/

int atoi(const char *s);

/* strtol.c */

long strtol(const char *nptr, char **endptr, int base);
unsigned long strtoul(const char *nptr, char **endptr, int base);

/* states.c */

const char *statename(unsigned state);

#endif
