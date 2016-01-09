/*
 * C-Brick Serial Port (and console) driver for SGI Altix machines.
 *
 * This driver is NOT suitable for talking to the l1-controller for
 * anything other than 'console activities' --- please use the l1
 * driver for that.
 *
 *
 * Copyright (c) 2003 Silicon Graphics, Inc.  All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it would be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Further, this software is distributed without any warranty that it is
 * free of the rightful claim of any third person regarding infringement
 * or the like.  Any license provided herein, whether implied or
 * otherwise, applies only to this software file.  Patent licenses, if
 * any, provided herein do not apply to combinations of this program with
 * other software, or any other product whatsoever.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston MA 02111-1307, USA.
 *
 * Contact information:  Silicon Graphics, Inc., 1600 Amphitheatre Pkwy,
 * Mountain View, CA  94043, or:
 *
 * http://www.sgi.com
 *
 * For further information regarding this notice, see:
 *
 * http://oss.sgi.com/projects/GenInfo/NoticeExplan
 */

#include <linux/config.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/tty.h>
#include <linux/serial.h>
#include <linux/console.h>
#include <linux/module.h>
#ifdef CONFIG_MAGIC_SYSRQ
#include <linux/sysrq.h>
#endif
#include <linux/string.h>
#include <linux/circ_buf.h>
#include <linux/serial_reg.h>
#include <asm/irq.h>
#include <asm/system.h>
#include <asm/io.h>
#include <asm/sn/sn_sal.h>
#include <asm/sn/nodepda.h>
#include <asm/sn/simulator.h>
#include <asm/sn/sn2/intr.h>
#include <asm/sn/sn2/sn_private.h>
#include <asm/sn/clksupport.h>

#if defined(CONFIG_SGI_L1_SERIAL_CONSOLE) && defined(CONFIG_MAGIC_SYSRQ)
static char sysrq_serial_str[] = "\eSYS";
static char *sysrq_serial_ptr = sysrq_serial_str;
static unsigned long sysrq_requested;
#endif /* CONFIG_SGI_L1_SERIAL_CONSOLE && CONFIG_MAGIC_SYSRQ */

static char *serial_revdate = "2003-07-31";

/* driver subtype - what does this mean? */
#define SN_SAL_SUBTYPE 1

/* minor device number */
#define SN_SAL_MINOR 64

/* number of characters left in xmit buffer before we ask for more */
#define WAKEUP_CHARS 128

/* number of characters we can transmit to the SAL console at a time */
#define SN_SAL_MAX_CHARS 120

/* event types for our task queue -- so far just one */
#define SN_SAL_EVENT_WRITE_WAKEUP 0

#define CONSOLE_RESTART 1

/* 64K, when we're asynch, it must be at least printk's LOG_BUF_LEN to
 * avoid losing chars, (always has to be a power of 2) */
#if 1
#define SN_SAL_BUFFER_SIZE (64 * (1 << 10))
#else
#define SN_SAL_BUFFER_SIZE (64)
#endif

#define SN_SAL_UART_FIFO_DEPTH 16
#define SN_SAL_UART_FIFO_SPEED_CPS 9600/10

/* we don't kmalloc/get_free_page these as we want them available
 * before either of those are initialized */
static volatile char xmit_buff_mem[SN_SAL_BUFFER_SIZE];

struct volatile_circ_buf {
	volatile char *buf;
	int head;
	int tail;
};

static volatile struct volatile_circ_buf xmit = { .buf = xmit_buff_mem };
static char sal_tmp_buffer[SN_SAL_BUFFER_SIZE];

static volatile struct tty_struct *sn_sal_tty;

static struct timer_list sn_sal_timer;
static int sn_sal_event; /* event type for task queue */
static int sn_sal_refcount;

static volatile int sn_sal_is_asynch;
static volatile int sn_sal_irq;
static spinlock_t sn_sal_lock = SPIN_LOCK_UNLOCKED;
static volatile int tx_count;
static volatile int rx_count;

static struct tty_struct *sn_sal_table;
static struct termios *sn_sal_termios;
static struct termios *sn_sal_termios_locked;

static void sn_sal_tasklet_action(unsigned long data);
static DECLARE_TASKLET(sn_sal_tasklet, sn_sal_tasklet_action, 0);

static volatile unsigned long interrupt_timeout;

extern u64 master_node_bedrock_address;

int debug_printf(const char *fmt, ...);

#undef DEBUG
#ifdef DEBUG
#define DPRINTF(x...) debug_printf(x)
#else
#define DPRINTF(x...) do { } while (0)
#endif

static void intr_transmit_chars(void);
static void poll_transmit_chars(void);
static int sn_sal_write(struct tty_struct *tty, int from_user,
			const unsigned char *buf, int count);

struct sn_sal_ops {
	int (*puts)(const char *s, int len);
	int (*getc)(void);
	int (*input_pending)(void);
	void (*wakeup_transmit)(void);
};

static volatile struct sn_sal_ops *sn_sal_ops;


/* the console does output in two distinctly different ways:
 * synchronous and asynchronous (buffered).  initally, early_printk
 * does synchronous output.  any data written goes directly to the SAL
 * to be output (incidentally, it is internally buffered by the SAL)
 * after interrupts and timers are initialized and available for use,
 * the console init code switches to asynchronous output.  this is
 * also the earliest opportunity to begin polling for console input.
 * after console initialization, console output and tty (serial port)
 * output is buffered and sent to the SAL asynchronously (either by
 * timer callback or by UART interrupt) */


/* routines for running the console in polling mode */

static int hw_puts(const char *s, int len)
{
	/* looking at the PROM source code, putb calls the flush
	 * routine, so if we send characters in FIFO sized chunks, it
	 * should go out by the next time the timer gets called */
	return ia64_sn_console_putb(s, len);
}

static int poll_getc(void)
{
	int ch;
	ia64_sn_console_getc(&ch);
	return ch;
}

static int poll_input_pending(void)
{
	int status, input;

	status = ia64_sn_console_check(&input);
	return !status && input;
}

static struct sn_sal_ops poll_ops = {
	.puts = hw_puts,
	.getc = poll_getc,
	.input_pending = poll_input_pending
};


/* routines for running the console on the simulator */

static int sim_puts(const char *str, int count)
{
	int counter = count;

#ifdef FLAG_DIRECT_CONSOLE_WRITES
	/* This is an easy way to pre-pend the output to know whether the output
	 * was done via sal or directly */
	writeb('[', master_node_bedrock_address + (UART_TX << 3));
	writeb('+', master_node_bedrock_address + (UART_TX << 3));
	writeb(']', master_node_bedrock_address + (UART_TX << 3));
	writeb(' ', master_node_bedrock_address + (UART_TX << 3));
#endif /* FLAG_DIRECT_CONSOLE_WRITES */
	while (counter > 0) {
		writeb(*str, master_node_bedrock_address + (UART_TX << 3));
		counter--;
		str++;
	}

	return count;
}

static int sim_getc(void)
{
	return readb(master_node_bedrock_address + (UART_RX << 3));
}

static int sim_input_pending(void)
{
	return readb(master_node_bedrock_address + (UART_LSR << 3)) & UART_LSR_DR;
}

static struct sn_sal_ops sim_ops = {
	.puts = sim_puts,
	.getc = sim_getc,
	.input_pending = sim_input_pending
};


/* routines for an interrupt driven console (normal) */

static int intr_getc(void)
{
	return ia64_sn_console_readc();
}

static int intr_input_pending(void)
{
	return ia64_sn_console_intr_status() & SAL_CONSOLE_INTR_RECV;
}

static struct sn_sal_ops intr_ops = {
	.puts = hw_puts,
	.getc = intr_getc,
	.input_pending = intr_input_pending,
	.wakeup_transmit = intr_transmit_chars
};

extern void early_sn_setup(void);

void
early_printk_sn_sal(const char *s, unsigned count)
{
	if (!sn_sal_ops) {
		if (IS_RUNNING_ON_SIMULATOR())
			sn_sal_ops = &sim_ops;
		else
			sn_sal_ops = &poll_ops;

		early_sn_setup();
	}
	sn_sal_ops->puts(s, count);
}

/* this is as "close to the metal" as we can get, used when the driver
 * itself may be broken */
int debug_printf(const char *fmt, ...)
{
	static char printk_buf[1024];
	int printed_len;
	va_list args;
	va_start(args, fmt);
	printed_len = vsnprintf(printk_buf, sizeof(printk_buf), fmt, args);
	early_printk_sn_sal(printk_buf, printed_len);
	va_end(args);
	return printed_len;
}

/*********************************************************************
 * Interrupt handling routines.
 */

static void
sn_sal_sched_event(int event)
{
	sn_sal_event |= (1 << event);
	tasklet_schedule(&sn_sal_tasklet);
}

/* receive_chars can be called before sn_sal_tty is initialized.  in
 * that case, its only use is to trigger sysrq and kdb */
static void
receive_chars(struct pt_regs *regs)
{
	int ch;

	while (sn_sal_ops->input_pending()) {
		ch = sn_sal_ops->getc();
		if (ch < 0) {
			printk(KERN_ERR "sn_serial: An error occured while "
			       "obtaining data from the console (0x%0x)\n", ch);
			break;
		}

#if defined(CONFIG_SGI_L1_SERIAL_CONSOLE) && defined(CONFIG_MAGIC_SYSRQ)
		if (sysrq_requested) {
			unsigned long sysrq_timeout = sysrq_requested + HZ*5;
			sysrq_requested = 0;
			if (ch && time_before(jiffies, sysrq_timeout)) {
				spin_unlock(&sn_sal_lock);
				handle_sysrq(ch, regs, NULL, NULL);
				spin_lock(&sn_sal_lock);
				/* don't record this char */
				continue;
			}
		}
		if (ch == *sysrq_serial_ptr) {
			if (!(*++sysrq_serial_ptr)) {
				sysrq_requested = jiffies;
				sysrq_serial_ptr = sysrq_serial_str;
			}
		} else
			sysrq_serial_ptr = sysrq_serial_str;
#endif /* CONFIG_SGI_L1_SERIAL_CONSOLE && CONFIG_MAGIC_SYSRQ */

		/* record the character to pass up to the tty layer */
		if (sn_sal_tty) {
			*sn_sal_tty->flip.char_buf_ptr = ch;
			sn_sal_tty->flip.char_buf_ptr++;
			sn_sal_tty->flip.count++;
			if (sn_sal_tty->flip.count == TTY_FLIPBUF_SIZE)
				break;
		}
		rx_count++;
	}

	if (sn_sal_tty)
		tty_flip_buffer_push(sn_sal_tty);
}


/* synch_flush_xmit must be called with sn_sal_lock */
static void
synch_flush_xmit(void)
{
	int xmit_count, tail, head, loops, ii;
	int result;
	char *start;

	if (xmit.head == xmit.tail)
		/* Nothing to do. */
		return;

	head = xmit.head;
	tail = xmit.tail;
	start = &xmit.buf[tail];

	/* twice around gets the tail to the end of the buffer and
	 * then to the head, if needed */
	loops = (head < tail) ? 2 : 1;

	for (ii = 0; ii < loops; ii++) {
		xmit_count = (head < tail) ?
			(SN_SAL_BUFFER_SIZE - tail) : (head - tail);

		if (xmit_count > 0) {
			result = sn_sal_ops->puts(start, xmit_count);
			if (!result)
				debug_printf("\n*** synch_flush_xmit failed to flush\n");
			if (result > 0) {
				xmit_count -= result;
				tx_count += result;
				tail += result;
				tail &= SN_SAL_BUFFER_SIZE - 1;
				xmit.tail = tail;
				start = &xmit.buf[tail];
			}
		}
	}

}

/* must be called with a lock protecting the circular buffer and
 * sn_sal_tty */
static void
poll_transmit_chars(void)
{
	int xmit_count, tail, head;
	int result;
	char *start;

	BUG_ON(!sn_sal_is_asynch);

	if (xmit.head == xmit.tail ||
	    (sn_sal_tty && (sn_sal_tty->stopped || sn_sal_tty->hw_stopped))) {
		/* Nothing to do. */
		return;
	}

	head = xmit.head;
	tail = xmit.tail;
	start = &xmit.buf[tail];

	xmit_count = (head < tail) ?
		(SN_SAL_BUFFER_SIZE - tail) : (head - tail);

	if (xmit_count == 0)
		debug_printf("\n*** empty xmit_count\n");

	if (xmit_count > SN_SAL_UART_FIFO_DEPTH)
		xmit_count = SN_SAL_UART_FIFO_DEPTH;
	
	/* use the ops, as we could be on the simulator */
	result = sn_sal_ops->puts(start, xmit_count);
	if (!result)
		debug_printf("\n*** error in synchronous puts\n");
	/* XXX chadt clean this up */
	if (result > 0) {
		xmit_count -= result;
		tx_count += result;
		tail += result;
		tail &= SN_SAL_BUFFER_SIZE - 1;
		xmit.tail = tail;
		start = &xmit.buf[tail];
	}

	/* if there's few enough characters left in the xmit buffer
	 * that we could stand for the upper layer to send us some
	 * more, ask for it. */
	if (sn_sal_tty)
		if (CIRC_CNT(xmit.head, xmit.tail, SN_SAL_BUFFER_SIZE) < WAKEUP_CHARS)
			sn_sal_sched_event(SN_SAL_EVENT_WRITE_WAKEUP);
}


/* must be called with a lock protecting the circular buffer and
 * sn_sal_tty */
static void
intr_transmit_chars(void)
{
	int xmit_count, tail, head, loops, ii;
	int result;
	char *start;

	BUG_ON(!sn_sal_is_asynch);

	if (xmit.head == xmit.tail ||
	    (sn_sal_tty && (sn_sal_tty->stopped || sn_sal_tty->hw_stopped))) {
		/* Nothing to do. */
		return;
	}

	head = xmit.head;
	tail = xmit.tail;
	start = &xmit.buf[tail];

	/* twice around gets the tail to the end of the buffer and
	 * then to the head, if needed */
	loops = (head < tail) ? 2 : 1;

	for (ii = 0; ii < loops; ii++) {
		xmit_count = (head < tail) ?
			(SN_SAL_BUFFER_SIZE - tail) : (head - tail);

		if (xmit_count > 0) {
			result = ia64_sn_console_xmit_chars(start, xmit_count);
#ifdef DEBUG
			if (!result)
				debug_printf("`");
#endif
			if (result > 0) {
				xmit_count -= result;
				tx_count += result;
				tail += result;
				tail &= SN_SAL_BUFFER_SIZE - 1;
				xmit.tail = tail;
				start = &xmit.buf[tail];
			}
		}
	}

	/* if there's few enough characters left in the xmit buffer
	 * that we could stand for the upper layer to send us some
	 * more, ask for it. */
	if (sn_sal_tty)
		if (CIRC_CNT(xmit.head, xmit.tail, SN_SAL_BUFFER_SIZE) < WAKEUP_CHARS)
			sn_sal_sched_event(SN_SAL_EVENT_WRITE_WAKEUP);
}


static void
sn_sal_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	/* this call is necessary to pass the interrupt back to the
	 * SAL, since it doesn't intercept the UART interrupts
	 * itself */
	int status = ia64_sn_console_intr_status();

	spin_lock(&sn_sal_lock);
	if (status & SAL_CONSOLE_INTR_RECV)
		receive_chars(regs);
	if (status & SAL_CONSOLE_INTR_XMIT)
		intr_transmit_chars();
	spin_unlock(&sn_sal_lock);
}


/* returns the console irq if interrupt is successfully registered,
 * else 0 */
static int
sn_sal_connect_interrupt(void)
{
	cpuid_t intr_cpuid;
	unsigned int intr_cpuloc;
	nasid_t console_nasid;
	unsigned int console_irq;
	int result;

	/* if it is an old prom, run in poll mode */
	if ((sn_sal_rev_major() <= 1) && (sn_sal_rev_minor() <= 3)) {
		/* before version 1.06 doesn't work */
		printk(KERN_INFO "sn_serial: old prom version %x.%02x"
		       " - running in polled mode\n",
		       sn_sal_rev_major(), sn_sal_rev_minor());
		return 0;
	}

	console_nasid = ia64_sn_get_console_nasid();
	intr_cpuid = NODEPDA(NASID_TO_COMPACT_NODEID(console_nasid))
		->node_first_cpu;
	intr_cpuloc = cpu_physical_id(intr_cpuid);
	console_irq = CPU_VECTOR_TO_IRQ(intr_cpuloc, SGI_UART_VECTOR);

	result = intr_connect_level(intr_cpuid, SGI_UART_VECTOR,
				    0 /*not used*/, 0 /*not used*/);
	BUG_ON(result != SGI_UART_VECTOR);

	result = request_irq(console_irq, sn_sal_interrupt,
			     SA_INTERRUPT,  "SAL console driver", &sn_sal_tty);
	if (result >= 0)
		return console_irq;

	printk(KERN_INFO "sn_serial: console proceeding in polled mode\n");
	return 0;
}

/*
 * End of the interrupt routines.
 *********************************************************************/


static void
sn_sal_tasklet_action(unsigned long data)
{
	unsigned long flags;

	if (sn_sal_tty) {
		spin_lock_irqsave(&sn_sal_lock, flags);
		if (sn_sal_tty)
			if (test_and_clear_bit(SN_SAL_EVENT_WRITE_WAKEUP, &sn_sal_event)) {
				if ((sn_sal_tty->flags & (1 << TTY_DO_WRITE_WAKEUP)) &&
				    sn_sal_tty->ldisc.write_wakeup)
					(sn_sal_tty->ldisc.write_wakeup)(sn_sal_tty);
				wake_up_interruptible(&sn_sal_tty->write_wait);
			}
		spin_unlock_irqrestore(&sn_sal_lock, flags);
	}
}


/*
 * This function handles polled mode.
 */
static void
sn_sal_timer_poll(unsigned long dummy)
{
	if (!sn_sal_irq) {
		spin_lock(&sn_sal_lock);
		receive_chars(NULL);
		poll_transmit_chars();
		spin_unlock(&sn_sal_lock);
		mod_timer(&sn_sal_timer, jiffies + interrupt_timeout);
	}
}

static void
sn_sal_timer_restart(unsigned long dummy)
{
	unsigned long flags;

	local_irq_save(flags);
	sn_sal_interrupt(0, NULL, NULL);
	local_irq_restore(flags);
	mod_timer(&sn_sal_timer, jiffies + interrupt_timeout);
}

/*
 * End of "sofware interrupt" routines.
 *********************************************************************/


/*********************************************************************
 * User-level console routines
 */

static int
sn_sal_open(struct tty_struct *tty, struct file *filp)
{
	unsigned long flags;

	DPRINTF("sn_sal_open: sn_sal_tty = %p, tty = %p, filp = %p\n",
		sn_sal_tty, tty, filp);

	spin_lock_irqsave(&sn_sal_lock, flags);
	if (!sn_sal_tty)
		sn_sal_tty = tty;
	spin_unlock_irqrestore(&sn_sal_lock, flags);

	return 0;
}


/* We're keeping all our resources.  We're keeping interrupts turned
 * on.  Maybe just let the tty layer finish its stuff...? GMSH
 */
static void
sn_sal_close(struct tty_struct *tty, struct file * filp)
{
	if (atomic_read(&tty->count) == 1) {
		unsigned long flags;
		tty->closing = 1;
		if (tty->driver.flush_buffer)
			tty->driver.flush_buffer(tty);
		if (tty->ldisc.flush_buffer)
			tty->ldisc.flush_buffer(tty);
		tty->closing = 0;
		spin_lock_irqsave(&sn_sal_lock, flags);
		sn_sal_tty = NULL;
		spin_unlock_irqrestore(&sn_sal_lock, flags);
	}
}


static int
sn_sal_write(struct tty_struct *tty, int from_user,
	     const unsigned char *buf, int count)
{
	int c, ret = 0;
	unsigned long flags;

	if (from_user) {
		while (1) {
			int c1;
			c = CIRC_SPACE_TO_END(xmit.head, xmit.tail,
					      SN_SAL_BUFFER_SIZE);

			if (count < c)
				c = count;
			if (c <= 0)
				break;

			c -= copy_from_user(sal_tmp_buffer, buf, c);
			if (!c) {
				if (!ret)
					ret = -EFAULT;
				break;
			}

			/* Turn off interrupts and see if the xmit buffer has
			 * moved since the last time we looked.
			 */
			spin_lock_irqsave(&sn_sal_lock, flags);
			c1 = CIRC_SPACE_TO_END(xmit.head, xmit.tail,
					       SN_SAL_BUFFER_SIZE);

			if (c1 < c)
				c = c1;

			memcpy(xmit.buf + xmit.head, sal_tmp_buffer, c);
			xmit.head = ((xmit.head + c) & (SN_SAL_BUFFER_SIZE - 1));
			spin_unlock_irqrestore(&sn_sal_lock, flags);

			buf += c;
			count -= c;
			ret += c;
		}
	} else {
		/* The buffer passed in isn't coming from userland,
		 * so cut out the middleman (sal_tmp_buffer).
		 */
		spin_lock_irqsave(&sn_sal_lock, flags);
		while (1) {
			c = CIRC_SPACE_TO_END(xmit.head, xmit.tail,
					      SN_SAL_BUFFER_SIZE);

			if (count < c)
				c = count;
			if (c <= 0) {
				break;
			}
			memcpy(xmit.buf + xmit.head, buf, c);
			xmit.head = ((xmit.head + c) & (SN_SAL_BUFFER_SIZE - 1));
			buf += c;
			count -= c;
			ret += c;
		}
		spin_unlock_irqrestore(&sn_sal_lock, flags);
	}

	spin_lock_irqsave(&sn_sal_lock, flags);
	if (xmit.head != xmit.tail &&
	    !(tty && (tty->stopped || tty->hw_stopped)))
		if (sn_sal_ops->wakeup_transmit)
			sn_sal_ops->wakeup_transmit();
	spin_unlock_irqrestore(&sn_sal_lock, flags);

	return ret;
}


static void
sn_sal_put_char(struct tty_struct *tty, unsigned char ch)
{
	unsigned long flags;

	spin_lock_irqsave(&sn_sal_lock, flags);
	if (CIRC_SPACE(xmit.head, xmit.tail, SN_SAL_BUFFER_SIZE) != 0) {
		xmit.buf[xmit.head] = ch;
		xmit.head = (xmit.head + 1) & (SN_SAL_BUFFER_SIZE-1);
		sn_sal_ops->wakeup_transmit();
	}
	spin_unlock_irqrestore(&sn_sal_lock, flags);
}


static void
sn_sal_flush_chars(struct tty_struct *tty)
{
	unsigned long flags;

	spin_lock_irqsave(&sn_sal_lock, flags);
	if (CIRC_CNT(xmit.head, xmit.tail, SN_SAL_BUFFER_SIZE))
		if (sn_sal_ops->wakeup_transmit)
			sn_sal_ops->wakeup_transmit();
	spin_unlock_irqrestore(&sn_sal_lock, flags);
}


static int
sn_sal_write_room(struct tty_struct *tty)
{
	unsigned long flags;
	int space;

	spin_lock_irqsave(&sn_sal_lock, flags);
	space = CIRC_SPACE(xmit.head, xmit.tail, SN_SAL_BUFFER_SIZE);
	spin_unlock_irqrestore(&sn_sal_lock, flags);
	return space;
}


static int
sn_sal_chars_in_buffer(struct tty_struct *tty)
{
	unsigned long flags;
	int space;

	spin_lock_irqsave(&sn_sal_lock, flags);
	space = CIRC_CNT(xmit.head, xmit.tail, SN_SAL_BUFFER_SIZE);
	DPRINTF("<%d>", space);
	spin_unlock_irqrestore(&sn_sal_lock, flags);
	return space;
}


static int
sn_sal_ioctl(struct tty_struct *tty, struct file *file,
	       unsigned int cmd, unsigned long arg)
{
	/* nothing supported */

	return -ENOIOCTLCMD;
}


static void
sn_sal_flush_buffer(struct tty_struct *tty)
{
	unsigned long flags;

	/* drop everything */
	spin_lock_irqsave(&sn_sal_lock, flags);
	xmit.head = xmit.tail = 0;
	spin_unlock_irqrestore(&sn_sal_lock, flags);

	/* wake up tty level */
	wake_up_interruptible(&tty->write_wait);
	if ((tty->flags & (1 << TTY_DO_WRITE_WAKEUP)) &&
	    tty->ldisc.write_wakeup)
		(tty->ldisc.write_wakeup)(tty);
}


static void
sn_sal_hangup(struct tty_struct *tty)
{
	sn_sal_flush_buffer(tty);
}


static void
sn_sal_wait_until_sent(struct tty_struct *tty, int timeout)
{
	/* this is SAL's problem */
	DPRINTF("<sn_serial: should wait until sent>");
}


/*
 * sn_sal_read_proc
 *
 * Console /proc interface
 */

static int
sn_sal_read_proc(char *page, char **start, off_t off, int count,
		   int *eof, void *data)
{
	int len = 0;
	off_t	begin = 0;
	extern nasid_t get_console_nasid(void);

	len += sprintf(page, "sn_serial: revision:%s nasid:%d irq:%d tx:%d rx:%d\n",
		       serial_revdate, get_console_nasid(), sn_sal_irq,
		       tx_count, rx_count);
	*eof = 1;

	if (off >= len+begin)
		return 0;
	*start = page + (off-begin);

	return count < begin+len-off ? count : begin+len-off;
}


static struct tty_driver sn_sal_driver = {
	.magic = TTY_DRIVER_MAGIC,
	.driver_name = "sn_serial",
#if defined(CONFIG_DEVFS_FS)
	.name = "tts/%d",
#else
	.name = "ttyS",
#endif
	.major = TTY_MAJOR,
	.minor_start = SN_SAL_MINOR,
	.num = 1,
	.type = TTY_DRIVER_TYPE_SERIAL,
	.subtype = SN_SAL_SUBTYPE,
	.flags = TTY_DRIVER_REAL_RAW | TTY_DRIVER_NO_DEVFS,
	.refcount = &sn_sal_refcount,
	.table = &sn_sal_table,
	.termios = &sn_sal_termios,
	.termios_locked = &sn_sal_termios_locked,

	.open = sn_sal_open,
	.close = sn_sal_close,
	.write = sn_sal_write,
	.put_char = sn_sal_put_char,
	.flush_chars = sn_sal_flush_chars,
	.write_room = sn_sal_write_room,
	.chars_in_buffer = sn_sal_chars_in_buffer,
	.ioctl = sn_sal_ioctl,
	.hangup = sn_sal_hangup,
	.wait_until_sent = sn_sal_wait_until_sent,
	.read_proc = sn_sal_read_proc,
};

/* sn_sal_init wishlist:
 * - allocate sal_tmp_buffer
 * - fix up the tty_driver struct
 * - turn on receive interrupts
 * - do any termios twiddling once and for all
 */

/*
 * Boot-time initialization code
 */

static void __init
sn_sal_switch_to_asynch(void)
{
	debug_printf("sn_serial: about to switch to asynchronous console\n");

	/* without early_printk, we may be invoked late enough to race
	 * with other cpus doing console IO at this point, however
	 * console interrupts will never be enabled */
	spin_lock(&sn_sal_lock);

	/* early_printk invocation may have done this for us */
	if (!sn_sal_ops) {
		if (IS_RUNNING_ON_SIMULATOR())
			sn_sal_ops = &sim_ops;
		else
			sn_sal_ops = &poll_ops;
	}

	/* we can't turn on the console interrupt (as request_irq
	 * calls kmalloc, which isn't set up yet), so we rely on a
	 * timer to poll for input and push data from the console
	 * buffer.
	 */
	init_timer(&sn_sal_timer);
	sn_sal_timer.function = sn_sal_timer_poll;

	if (IS_RUNNING_ON_SIMULATOR())
		interrupt_timeout = 6;
	else
		/* 960cps / 16 char FIFO = 60HZ 
		   HZ / (SN_SAL_FIFO_SPEED_CPS / SN_SAL_FIFO_DEPTH) */
		interrupt_timeout = HZ * SN_SAL_UART_FIFO_DEPTH / 
			SN_SAL_UART_FIFO_SPEED_CPS;

	mod_timer(&sn_sal_timer, jiffies + interrupt_timeout);

	sn_sal_is_asynch = 1;
	spin_unlock(&sn_sal_lock);
}

static void __init
sn_sal_switch_to_interrupts(void)
{
	int irq;

	debug_printf("sn_serial: switching to interrupt driven console\n");

	irq = sn_sal_connect_interrupt();
	if (irq) {
		unsigned long flags;
		spin_lock_irqsave(&sn_sal_lock, flags);
		/* sn_sal_irq is a global variable.  When it's set to
		 * a non-zero value, we stop polling for input (since
		 * interrupts should now be enabled). */
		sn_sal_irq = irq;
		sn_sal_ops = &intr_ops;
		/* turn on receive interrupts */
		ia64_sn_console_intr_enable(SAL_CONSOLE_INTR_RECV);

		/* the polling timer is already set up, we just change the
		 * frequency.  if we've successfully enabled interrupts (and
		 * CONSOLE_RESTART isn't defined) the next timer expiration
		 * will be the last, otherwise we continue polling */
		if (CONSOLE_RESTART) {
			/* kick the console every once in a while in
			 * case we miss an interrupt */
			interrupt_timeout = 20*HZ;
			sn_sal_timer.function = sn_sal_timer_restart;
			mod_timer(&sn_sal_timer, jiffies + interrupt_timeout);
		}
		spin_unlock_irqrestore(&sn_sal_lock, flags);
	}
}

static int __init
sn_sal_module_init(void)
{
	int retval;

	printk("sn_serial: sn_sal_module_init\n");

	if (!ia64_platform_is("sn2"))
		return -ENODEV;

	/* when this driver is compiled in, the console initialization
	 * will have already switched us into asynchronous operation
	 * before we get here through the module initcalls */
	if (!sn_sal_is_asynch)
		sn_sal_switch_to_asynch();

	/* at this point (module_init) we can try to turn on
	 * interrupts */
	if (!IS_RUNNING_ON_SIMULATOR())
	    sn_sal_switch_to_interrupts();

	sn_sal_driver.init_termios = tty_std_termios;
	sn_sal_driver.init_termios.c_cflag = B9600 | CS8 | CREAD | HUPCL | CLOCAL;

	if ((retval = tty_register_driver(&sn_sal_driver))) {
		printk(KERN_ERR "sn_serial: Unable to register tty driver\n");
		return retval;
	}

	tty_register_devfs(&sn_sal_driver, 0, sn_sal_driver.minor_start);

	return 0;
}


static void __exit
sn_sal_module_exit(void)
{
	unsigned long flags;
	int e;

	del_timer_sync(&sn_sal_timer);
	spin_lock_irqsave(&sn_sal_lock, flags);
	if ((e = tty_unregister_driver(&sn_sal_driver)))
		printk(KERN_ERR "sn_serial: failed to unregister driver (%d)\n", e);

	spin_unlock_irqrestore(&sn_sal_lock, flags);
}

module_init(sn_sal_module_init);
module_exit(sn_sal_module_exit);

/*
 * End of user-level console routines.
 *********************************************************************/


/*********************************************************************
 * Kernel console definitions
 */

#ifdef CONFIG_SGI_L1_SERIAL_CONSOLE
/*
 * Print a string to the SAL console.  The console_lock must be held
 * when we get here.
 */
static void
sn_sal_console_write(struct console *co, const char *s, unsigned count)
{
	BUG_ON(!sn_sal_is_asynch);

	if (count > CIRC_SPACE_TO_END(xmit.head, xmit.tail,
				      SN_SAL_BUFFER_SIZE))
		debug_printf("\n*** SN_SAL_BUFFER_SIZE too small, lost chars\n");

	/* somebody really wants this output, might be an
	 * oops, kdb, panic, etc.  make sure they get it. */
	if (spin_is_locked(&sn_sal_lock)) {
		synch_flush_xmit();
		sn_sal_ops->puts(s, count);
	} else if (in_interrupt()) {
		spin_lock(&sn_sal_lock);
		synch_flush_xmit();
		spin_unlock(&sn_sal_lock);
		sn_sal_ops->puts(s, count);
	} else
		sn_sal_write(NULL, 0, s, count);
}

static kdev_t
sn_sal_console_device(struct console *c)
{
	return MKDEV(TTY_MAJOR, 64 + c->index);
}

static int __init
sn_sal_console_setup(struct console *co, char *options)
{
	return 0;
}


static struct console sal_console = {
	.name = "ttyS",
	.write = sn_sal_console_write,
	.device = sn_sal_console_device,
	.setup = sn_sal_console_setup,
	.index = -1
};

/*
 * End of kernel console definitions.
 *********************************************************************/


void __init
sn_sal_serial_console_init(void)
{
	if (ia64_platform_is("sn2")) {
		sn_sal_switch_to_asynch();
		register_console(&sal_console);
	}
}

#endif /* CONFIG_SGI_L1_SERIAL_CONSOLE */
