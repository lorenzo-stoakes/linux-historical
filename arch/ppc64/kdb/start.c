/*
 * Copyright (C) 1996 Paul Mackerras.
 */
#include <linux/config.h>
#include <linux/string.h>
#include <linux/fs.h>
#include <asm/machdep.h>
#include <asm/io.h>
#include <asm/page.h>
#include <linux/adb.h>
#include <linux/pmu.h>
#include <linux/cuda.h>
#include <linux/kernel.h>
#include <asm/prom.h>

#include <linux/sysrq.h>
#include <linux/kdb.h>
#include <asm/kdb.h>

#include <asm/processor.h>
#include <asm/delay.h>
#ifdef CONFIG_SMP
#include <asm/bitops.h>
#endif

/* kdb will use UDBG */
#define USE_UDBG

#ifdef USE_UDBG
#include <asm/udbg.h>
#endif



static void sysrq_handle_kdb(int key, struct pt_regs *pt_regs, struct kbd_struct *kbd, struct tty_struct *tty) 
{
  kdb(KDB_REASON_KEYBOARD,0,pt_regs);
}

static struct sysrq_key_op sysrq_kdb_op = 
{
	handler:	sysrq_handle_kdb,
	help_msg:	"kdb",
	action_msg:	"Entering kdb\n",
};



void
kdb_map_scc(void)
{
	/* register sysrq 'x' */
	__sysrq_put_key_op('x', &sysrq_kdb_op);
}


