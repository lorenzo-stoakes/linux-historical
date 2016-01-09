/*
 * Kernel Debugger Console I/O handler
 *
 * Copyright (C) 1999 Silicon Graphics, Inc.
 * Copyright (C) Scott Lurndal (slurn@engr.sgi.com)
 * Copyright (C) Scott Foehner (sfoehner@engr.sgi.com)
 * Copyright (C) Srinivasa Thirumalachar (sprasad@engr.sgi.com)
 *
 * See the file LIA-COPYRIGHT for additional information.
 *
 * Written March 1999 by Scott Lurndal at Silicon Graphics, Inc.
 *
 * Modifications from:
 *	Chuck Fleckenstein		1999/07/20
 *		Move kdb_info struct declaration to this file
 *		for cases where serial support is not compiled into
 *		the kernel.
 *
 *	Masahiro Adegawa		1999/07/20
 *		Handle some peculiarities of japanese 86/106
 *		keyboards.
 *
 *	marc@mucom.co.il		1999/07/20
 *		Catch buffer overflow for serial input.
 *
 *      Scott Foehner
 *              Port to ia64
 *
 *	Scott Lurndal			2000/01/03
 *		Restructure for v1.0
 *
 *	Keith Owens			2000/05/23
 *		KDB v1.2
 *
 *	Andi Kleen			2000/03/19
 *		Support simultaneous input from serial line and keyboard.
 */

#include <linux/kernel.h>
#include <asm/io.h>
#include <linux/wait.h>
#include <linux/delay.h>
#include <linux/pc_keyb.h>
#include <linux/console.h>
#include <linux/ctype.h>
#include <linux/keyboard.h>
#include <linux/serial_reg.h>

#include <linux/kdb.h>
#include <linux/kdbprivate.h>
#include <asm/machdep.h>
#undef FILE

int kdb_port;
int inchar(void);

/*
 * This module contains code to read characters from the keyboard or a serial
 * port.
 *
 * It is used by the kernel debugger, and is polled, not interrupt driven.
 *
 */
void
kdb_resetkeyboard(void)
{
#if 0
	kdb_kbdsend(KBD_CMD_ENABLE);
#endif
}


#if 0
/* code that may be resurrected later.. */
#if defined(CONFIG_VT)
/*
 * Check if the keyboard controller has a keypress for us.
 * Some parts (Enter Release, LED change) are still blocking polled here,
 * but hopefully they are all short.
 */
static int get_kbd_char(void)
{
	int keychar;
/*  	keychar = inchar(); */
	keychar = ppc_md.udbg_getc_poll();

	if (keychar == '\n')
	{
	    kdb_printf("\n");
	}

	/*
	 * echo the character.
	 */
	kdb_printf("%c", keychar);

	return keychar ;
}
#endif /* CONFIG_VT */
#endif

static int get_char_lp(void)
{
    int keychar;
    keychar = ppc_md.udbg_getc_poll();

    if (keychar == '\n')
    {
	kdb_printf("\n");
    }

#if 0
    /* echo the character. */
    if (keychar != -1)
	kdb_printf("%c", keychar);
#endif

    return keychar ;
}


char *
kdba_read(char *buffer, size_t bufsize)
{
	char	*cp = buffer;
	char	*bufend = buffer+bufsize-2;	/* Reserve space for newline and null byte */

	for (;;) {
	    unsigned char key = ppc_md.udbg_getc();
		/* Echo is done in the low level functions */
		switch (key) {
		case '\b': /* backspace */
		case '\x7f': /* delete */
			if (cp > buffer) {
				udbg_puts("\b \b");
				--cp;
			}
			break;
		case '\n': /* enter */
		case '\r': /* - the other enter... */
			ppc_md.udbg_putc('\n');
			*cp++ = '\n';
			*cp++ = '\0';
			return buffer;
		default:
			if (cp < bufend)
			ppc_md.udbg_putc(key);
				*cp++ = key;
			break;
		}
	}
}



