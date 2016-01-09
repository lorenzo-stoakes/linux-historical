#ifndef NONSTDIO_H
#define NONSTDIO_H

#define EOF	(-1)
#if 0 
typedef int	FILE;
#endif

/* candidate for removal.. */
extern FILE *xmon_stdin, *xmon_stdout;
#if 0
#define stdin	xmon_stdin
#define stdout	xmon_stdout
#endif
#define printf	xmon_printf
#define fprintf	xmon_fprintf
#define fputs	xmon_fputs
#define fgets	xmon_fgets
#define putchar	xmon_putchar
#define getchar	xmon_getchar
#define putc	xmon_putc
#define getc	xmon_getc
#define fopen(n, m)	NULL
#define fflush(f)	do {} while (0)
#define fclose(f)	do {} while (0)
extern char *fgets(char *, int, void *);
extern void xmon_printf(const char *, ...);
extern void xmon_fprintf(void *, const char *, ...);
extern void xmon_sprintf(char *, const char *, ...);
#define perror(s)	printf("%s: no files!\n", (s))

#endif
