/*
 * Kernel Debugger Architecture Independent Support Functions
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
 *      Richard Bass                    1999/07/20
 *              Many bug fixes and enhancements.
 *      Scott Foehner
 *              Port to ia64
 *	Scott Lurndal			1999/12/12
 *		v1.0 restructuring.
 *	Keith Owens			2000/05/23
 *		KDB v1.2
 */

#include <linux/string.h>
#include <linux/stddef.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/ptrace.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/kdb.h>
#include <linux/kdbprivate.h>

#include <asm/processor.h>
#include "privinst.h"
#include <asm/uaccess.h>
#include <asm/machdep.h>

extern const char *kdb_diemsg;


/* prototypes */
int valid_ppc64_kernel_address(unsigned long addr, unsigned long size);
extern int kdba_excprint(int argc, const char **argv, const char **envp, struct pt_regs *regs);
extern int kdba_super_regs(int argc, const char **argv, const char **envp, struct pt_regs *regs);
extern int kdba_dissect_msr(int argc, const char **argv, const char **envp, struct pt_regs *regs);
extern int kdba_halt(int argc, const char **argv, const char **envp, struct pt_regs *regs);
extern int kdba_dump_tce_table(int argc, const char **argv, const char **envp, struct pt_regs *regs);
extern int kdba_kernelversion(int argc, const char **argv, const char **envp, struct pt_regs *regs);
extern int kdba_dmesg(int argc, const char **argv, const char **envp, struct pt_regs *regs);
extern int kdba_dump_pci_info(int argc, const char **argv, const char **envp, struct pt_regs *regs);
unsigned long kdba_getword(unsigned long addr, size_t width);

/*
 * kdba_init
 * 	Architecture specific initialization.
 */
/*
kdb_register("commandname",              # name of command user will use to invoke function  
             function_name,              # name of function within the code 
             "function example usage",   # sample usage 
             "function description",     # brief description. 
             0                           # if i hit enter again, will command repeat itself ?
Note: functions must take parameters as such:
functionname(int argc, const char **argv, const char **envp, struct pt_regs *regs)
*/

void __init
kdba_init(void)
{
	kdba_enable_lbr();
	kdb_register("excp", kdba_excprint, "excp", "print exception info", 0);
	kdb_register("superreg", kdba_super_regs, "superreg", "display super_regs", 0);
	kdb_register("msr", kdba_dissect_msr, "msr", "dissect msr", 0);
	kdb_register("halt", kdba_halt, "halt", "halt machine", 0);
	kdb_register("dump_tce_table", kdba_dump_tce_table, "dump_tce_table <addr> [full]", "dump the tce table located at <addr>", 0);
	kdb_register("kernel", kdba_kernelversion, "version", "display running kernel version", 0);
	kdb_register("version", kdba_kernelversion, "version", "display running kernel version", 0);
	kdb_register("_dmesg", kdba_dmesg, "dmesg <lines>", "display lines from dmesg (log_buf) buffer", 0);
	kdb_register("pci_info", kdba_dump_pci_info, "dump_pci_info", "dump pci device info", 0);


	if (!ppc_md.udbg_getc_poll)
		kdb_on = 0;
}




/*
 * kdba_prologue
 *
 *	This function analyzes a gcc-generated function prototype
 *	with or without frame pointers to determine the amount of
 *	automatic storage and register save storage is used on the
 *	stack of the target function.  It only counts instructions
 *	that have been executed up to but excluding the current eip.
 * Inputs:
 *	code	Start address of function code to analyze
 *	pc	Current program counter within function
 *	sp	Current stack pointer for function
 *	fp	Current frame pointer for function, may not be valid
 *	ss	Start of stack for current process.
 *	caller	1 if looking for data on the caller frame, 0 for callee.
 * Outputs:
 *	ar	Activation record, all fields may be set.  fp and oldfp
 *		are 0 if they cannot be extracted.  return is 0 if the
 *		code cannot find a valid return address.  args and arg0
 *		are 0 if the number of arguments cannot be safely
 *		calculated.
 * Returns:
 *	1 if prologue is valid, 0 otherwise.  If pc is 0 treat it as a
 *	valid prologue to allow bt on wild branches.
 * Locking:
 *	None.
 * Remarks:
 *
 */
int
kdba_prologue(const kdb_symtab_t *symtab, kdb_machreg_t pc, kdb_machreg_t sp,
	      kdb_machreg_t fp, kdb_machreg_t ss, int caller, kdb_ar_t *ar)
{
	/* We don't currently use kdb's generic activation record scanning
	 * code to handle backtrace.
	 */
	return 0;
}



/*
 * kdba_getregcontents
 *
 *	Return the contents of the register specified by the
 *	input string argument.   Return an error if the string
 *	does not match a machine register.
 *
 *	The following pseudo register names are supported:
 *	   &regs	 - Prints address of exception frame
 *	   kesp		 - Prints kernel stack pointer at time of fault
 *	   cesp		 - Prints current kernel stack pointer, inside kdb
 *	   ceflags	 - Prints current flags, inside kdb
 *	   %<regname>	 - Uses the value of the registers at the
 *			   last time the user process entered kernel
 *			   mode, instead of the registers at the time
 *			   kdb was entered.
 *
 * Parameters:
 *	regname		Pointer to string naming register
 *	regs		Pointer to structure containing registers.
 * Outputs:
 *	*contents	Pointer to unsigned long to recieve register contents
 * Returns:
 *	0		Success
 *	KDB_BADREG	Invalid register name
 * Locking:
 * 	None.
 * Remarks:
 * 	If kdb was entered via an interrupt from the kernel itself then
 *	ss and esp are *not* on the stack.
 */

static struct kdbregs {
	char   *reg_name;
	size_t	reg_offset;
} kdbreglist[] = {
	{ "gpr0",	offsetof(struct pt_regs, gpr[0]) },
	{ "gpr1",	offsetof(struct pt_regs, gpr[1]) },
	{ "gpr2",	offsetof(struct pt_regs, gpr[2]) },
	{ "gpr3",	offsetof(struct pt_regs, gpr[3]) },
	{ "gpr4",	offsetof(struct pt_regs, gpr[4]) },
	{ "gpr5",	offsetof(struct pt_regs, gpr[5]) },
	{ "gpr6",	offsetof(struct pt_regs, gpr[6]) },
	{ "gpr7",	offsetof(struct pt_regs, gpr[7]) },
	{ "gpr8",	offsetof(struct pt_regs, gpr[8]) },
	{ "gpr9",	offsetof(struct pt_regs, gpr[9]) },
	{ "gpr10",	offsetof(struct pt_regs, gpr[10]) },
	{ "gpr11",	offsetof(struct pt_regs, gpr[11]) },
	{ "gpr12",	offsetof(struct pt_regs, gpr[12]) },
	{ "gpr13",	offsetof(struct pt_regs, gpr[13]) },
	{ "gpr14",	offsetof(struct pt_regs, gpr[14]) },
	{ "gpr15",	offsetof(struct pt_regs, gpr[15]) },
	{ "gpr16",	offsetof(struct pt_regs, gpr[16]) },
	{ "gpr17",	offsetof(struct pt_regs, gpr[17]) },
	{ "gpr18",	offsetof(struct pt_regs, gpr[18]) },
	{ "gpr19",	offsetof(struct pt_regs, gpr[19]) },
	{ "gpr20",	offsetof(struct pt_regs, gpr[20]) },
	{ "gpr21",	offsetof(struct pt_regs, gpr[21]) },
	{ "gpr22",	offsetof(struct pt_regs, gpr[22]) },
	{ "gpr23",	offsetof(struct pt_regs, gpr[23]) },
	{ "gpr24",	offsetof(struct pt_regs, gpr[24]) },
	{ "gpr25",	offsetof(struct pt_regs, gpr[25]) },
	{ "gpr26",	offsetof(struct pt_regs, gpr[26]) },
	{ "gpr27",	offsetof(struct pt_regs, gpr[27]) },
	{ "gpr28",	offsetof(struct pt_regs, gpr[28]) },
	{ "gpr29",	offsetof(struct pt_regs, gpr[29]) },
	{ "gpr30",	offsetof(struct pt_regs, gpr[30]) },
	{ "gpr31",	offsetof(struct pt_regs, gpr[31]) },
	{ "eip",	offsetof(struct pt_regs, nip) },
	{ "msr",	offsetof(struct pt_regs, msr) },
	{ "esp",	offsetof(struct pt_regs, gpr[1]) },
  	{ "orig_gpr3",  offsetof(struct pt_regs, orig_gpr3) },
	{ "ctr", 	offsetof(struct pt_regs, ctr) },
	{ "link",	offsetof(struct pt_regs, link) },
	{ "xer", 	offsetof(struct pt_regs, xer) },
	{ "ccr",	offsetof(struct pt_regs, ccr) },
	{ "mq",		offsetof(struct pt_regs, softe) /* mq */ },
	{ "trap",	offsetof(struct pt_regs, trap) },
	{ "dar",	offsetof(struct pt_regs, dar)  },
	{ "dsisr",	offsetof(struct pt_regs, dsisr) },
	{ "result",	offsetof(struct pt_regs, result) },
};

static const int nkdbreglist = sizeof(kdbreglist) / sizeof(struct kdbregs);

unsigned long
getsp(void)
{
	unsigned long x;
	asm("mr %0,1" : "=r" (x):);
	return x;
}

int
kdba_getregcontents(const char *regname,
		    struct pt_regs *regs,
		    kdb_machreg_t *contents)
{
	int i;

	if (strcmp(regname, "&regs") == 0) {
		*contents = (unsigned long)regs;
		return 0;
	}

	if (strcmp(regname, "kesp") == 0) {
		*contents = (unsigned long) current->thread.ksp;
		return 0;
	}

	if (strcmp(regname, "cesp") == 0) {
		*contents = getsp();
		return 0;
	}

	if (strcmp(regname, "ceflags") == 0) {
		int flags;
		save_flags(flags);
		*contents = flags;
		return 0;
	}

	if (regname[0] == '%') {
		/* User registers:  %%e[a-c]x, etc */
		regname++;
		regs = (struct pt_regs *)
			(current->thread.ksp - sizeof(struct pt_regs));
	}

	for (i=0; i<nkdbreglist; i++) {
		if (strnicmp(kdbreglist[i].reg_name,
			     regname,
			     strlen(regname)) == 0)
			break;
	}

	if ((i < nkdbreglist)
	 && (strlen(kdbreglist[i].reg_name) == strlen(regname))) {
		*contents = *(unsigned long *)((unsigned long)regs +
				kdbreglist[i].reg_offset);
		return(0);
	}

	return KDB_BADREG;
}

/*
 * kdba_setregcontents
 *
 *	Set the contents of the register specified by the
 *	input string argument.   Return an error if the string
 *	does not match a machine register.
 *
 *	Supports modification of user-mode registers via
 *	%<register-name>
 *
 * Parameters:
 *	regname		Pointer to string naming register
 *	regs		Pointer to structure containing registers.
 *	contents	Unsigned long containing new register contents
 * Outputs:
 * Returns:
 *	0		Success
 *	KDB_BADREG	Invalid register name
 * Locking:
 * 	None.
 * Remarks:
 */

int
kdba_setregcontents(const char *regname,
		  struct pt_regs *regs,
		  unsigned long contents)
{
	int i;

	if (regname[0] == '%') {
		regname++;
		regs = (struct pt_regs *)
			(current->thread.ksp - sizeof(struct pt_regs));
	}

	for (i=0; i<nkdbreglist; i++) {
		if (strnicmp(kdbreglist[i].reg_name,
			     regname,
			     strlen(regname)) == 0)
			break;
	}

	if ((i < nkdbreglist)
	 && (strlen(kdbreglist[i].reg_name) == strlen(regname))) {
		*(unsigned long *)((unsigned long)regs
				   + kdbreglist[i].reg_offset) = contents;
		return 0;
	}

	return KDB_BADREG;
}

/*
 * kdba_dumpregs
 *
 *	Dump the specified register set to the display.
 *
 * Parameters:
 *	regs		Pointer to structure containing registers.
 *	type		Character string identifying register set to dump
 *	extra		string further identifying register (optional)
 * Outputs:
 * Returns:
 *	0		Success
 * Locking:
 * 	None.
 * Remarks:
 *	This function will dump the general register set if the type
 *	argument is NULL (struct pt_regs).   The alternate register
 *	set types supported by this function:
 *
 *	d 		Debug registers
 *	c		Control registers
 *	u		User registers at most recent entry to kernel
 * Following not yet implemented:
 *	m		Model Specific Registers (extra defines register #)
 *	r		Memory Type Range Registers (extra defines register)
 */

int
kdba_dumpregs(struct pt_regs *regs,
	    const char *type,
	    const char *extra)
{
	int i;
	int count = 0;

	if (type
	 && (type[0] == 'u')) {
		type = NULL;
		regs = (struct pt_regs *)
			(current->thread.ksp - sizeof(struct pt_regs));
	}

	if (type == NULL) {
		struct kdbregs *rlp;
		kdb_machreg_t contents;

		for (i=0, rlp=kdbreglist; i<nkdbreglist; i++,rlp++) {
			kdba_getregcontents(rlp->reg_name, regs, &contents);
			kdb_printf("%-5s = 0x%p%c", rlp->reg_name, (void *)contents, (++count % 2) ? ' ' : '\n');
		}

		kdb_printf("&regs = 0x%p\n", regs);

		return 0;
	}

	switch (type[0]) {
	case 'm':
		break;
	case 'r':
		break;
	default:
		return KDB_BADREG;
	}

	/* NOTREACHED */
	return 0;
}

kdb_machreg_t
kdba_getpc(kdb_eframe_t ef)
{
	return ef->nip;
}

int
kdba_setpc(kdb_eframe_t ef, kdb_machreg_t newpc)
{
/* for ppc64, newpc passed in is actually a function descriptor for kdb. */
    ef->nip =     kdba_getword(newpc+8, 8);
    KDB_STATE_SET(IP_ADJUSTED);
    return 0;
}

/*
 * kdba_main_loop
 *
 *	Do any architecture specific set up before entering the main kdb loop.
 *	The primary function of this routine is to make all processes look the
 *	same to kdb, kdb must be able to list a process without worrying if the
 *	process is running or blocked, so make all process look as though they
 *	are blocked.
 *
 * Inputs:
 *	reason		The reason KDB was invoked
 *	error		The hardware-defined error code
 *	error2		kdb's current reason code.  Initially error but can change
 *			acording to kdb state.
 *	db_result	Result from break or debug point.
 *	ef		The exception frame at time of fault/breakpoint.  If reason
 *			is KDB_REASON_SILENT then ef is NULL, otherwise it should
 *			always be valid.
 * Returns:
 *	0	KDB was invoked for an event which it wasn't responsible
 *	1	KDB handled the event for which it was invoked.
 * Outputs:
 *	Sets eip and esp in current->thread.
 * Locking:
 *	None.
 * Remarks:
 *	none.
 */

int
kdba_main_loop(kdb_reason_t reason, kdb_reason_t reason2, int error,
	       kdb_dbtrap_t db_result, kdb_eframe_t ef)
{
	int rv;
	unsigned int msr;
	if (current->thread.regs == NULL)
	{
		struct pt_regs regs;
		asm volatile ("std	0,0(%0)\n\
                               std	1,8(%0)\n\
                               std	2,16(%0)\n\
                               std	3,24(%0)\n\
                               std	4,32(%0)\n\
                               std	5,40(%0)\n\
                               std	6,48(%0)\n\
                               std	7,56(%0)\n\
                               std	8,64(%0)\n\
                               std	9,72(%0)\n\
                               std	10,80(%0)\n\
                               std	11,88(%0)\n\
                               std	12,96(%0)\n\
                               std	13,104(%0)\n\
                               std	14,112(%0)\n\
                               std	15,120(%0)\n\
                               std	16,128(%0)\n\
                               std	17,136(%0)\n\
                               std	18,144(%0)\n\
                               std	19,152(%0)\n\
                               std	20,160(%0)\n\
                               std	21,168(%0)\n\
                               std	22,176(%0)\n\
                               std	23,184(%0)\n\
                               std	24,192(%0)\n\
                               std	25,200(%0)\n\
                               std	26,208(%0)\n\
                               std	27,216(%0)\n\
                               std	28,224(%0)\n\
                               std	29,232(%0)\n\
                               std	30,240(%0)\n\
                               std	31,248(%0)" : : "b" (&regs));
		/* Fetch the link reg for this stack frame.
		 NOTE: the prev kdb_printf fills in the lr. */
		regs.nip = regs.link = ((unsigned long *)regs.gpr[1])[2];
		regs.msr = get_msr();
		regs.ctr = get_ctr();
		regs.xer = get_xer();
		regs.ccr = get_cr();
		regs.trap = 0;
		current->thread.regs = &regs;
	}
	if (ef) {
		kdba_getregcontents("eip", ef, &(current->thread.regs->nip));
		kdba_getregcontents("esp", ef, &(current->thread.regs->gpr[1]));
		
	}
	msr = get_msr();
	set_msr( msr & ~0x8000);
	rv = kdb_main_loop(reason, reason2, error, db_result, ef);
	set_msr(msr);
	return rv;
}

void
kdba_disableint(kdb_intstate_t *state)
{
	int *fp = (int *)state;
	int   flags;

	save_flags(flags);
	cli();

	*fp = flags;
}

void
kdba_restoreint(kdb_intstate_t *state)
{
	int flags = *(int *)state;
	restore_flags(flags);
}

void
kdba_setsinglestep(struct pt_regs *regs)
{
	regs->msr |= MSR_SE;
}

void
kdba_clearsinglestep(struct pt_regs *regs)
{
	
	regs->msr &= ~MSR_SE;
}

int
kdba_getcurrentframe(struct pt_regs *regs)
{
	regs->gpr[1] = getsp();
	/* this stack pointer becomes invalid after we return, so take another step back.  */
	regs->gpr[1] = kdba_getword(regs->gpr[1], 8);
	return 0;
}

#ifdef KDB_HAVE_LONGJMP
int kdba_setjmp(kdb_jmp_buf *buf)
{
    asm volatile (
	"mflr 0; std 0,0(%0)\n\
	 std	1,8(%0)\n\
	 std	2,16(%0)\n\
	 mfcr 0; std 0,24(%0)\n\
	 std	13,32(%0)\n\
	 std	14,40(%0)\n\
	 std	15,48(%0)\n\
	 std	16,56(%0)\n\
	 std	17,64(%0)\n\
	 std	18,72(%0)\n\
	 std	19,80(%0)\n\
	 std	20,88(%0)\n\
	 std	21,96(%0)\n\
	 std	22,104(%0)\n\
	 std	23,112(%0)\n\
	 std	24,120(%0)\n\
	 std	25,128(%0)\n\
	 std	26,136(%0)\n\
	 std	27,144(%0)\n\
	 std	28,152(%0)\n\
	 std	29,160(%0)\n\
	 std	30,168(%0)\n\
	 std	31,176(%0)\n\
	 " : : "r" (buf));
    KDB_STATE_SET(LONGJMP);
    return 0;
}
void kdba_longjmp(kdb_jmp_buf *buf, int val)
{
    if (val == 0)
	val = 1;
    asm volatile (
	"ld	13,32(%0)\n\
	 ld	14,40(%0)\n\
	 ld	15,48(%0)\n\
	 ld	16,56(%0)\n\
	 ld	17,64(%0)\n\
	 ld	18,72(%0)\n\
	 ld	19,80(%0)\n\
	 ld	20,88(%0)\n\
	 ld	21,96(%0)\n\
	 ld	22,104(%0)\n\
	 ld	23,112(%0)\n\
	 ld	24,120(%0)\n\
	 ld	25,128(%0)\n\
	 ld	26,136(%0)\n\
	 ld	27,144(%0)\n\
	 ld	28,152(%0)\n\
	 ld	29,160(%0)\n\
	 ld	30,168(%0)\n\
	 ld	31,176(%0)\n\
	 ld	0,24(%0)\n\
	 mtcrf	0x38,0\n\
	 ld	0,0(%0)\n\
	 ld	1,8(%0)\n\
	 ld	2,16(%0)\n\
	 mtlr	0\n\
	 mr	3,%1\n\
	 " : : "r" (buf), "r" (val));
}
#endif

/*
 * kdba_enable_mce
 *
 *	This function is called once on each CPU to enable machine
 *	check exception handling.
 *
 * Inputs:
 *	None.
 * Outputs:
 *	None.
 * Returns:
 *	None.
 * Locking:
 *	None.
 * Remarks:
 *
 */

void
kdba_enable_mce(void)
{
}

/*
 * kdba_enable_lbr
 *
 *	Enable last branch recording.
 *
 * Parameters:
 *	None.
 * Returns:
 *	None
 * Locking:
 *	None
 * Remarks:
 *	None.
 */

void
kdba_enable_lbr(void)
{
}

/*
 * kdba_disable_lbr
 *
 *	disable last branch recording.
 *
 * Parameters:
 *	None.
 * Returns:
 *	None
 * Locking:
 *	None
 * Remarks:
 *	None.
 */

void
kdba_disable_lbr(void)
{
}

/*
 * kdba_print_lbr
 *
 *	Print last branch and last exception addresses
 *
 * Parameters:
 *	None.
 * Returns:
 *	None
 * Locking:
 *	None
 * Remarks:
 *	None.
 */

void
kdba_print_lbr(void)
{
}

/*
 * kdba_getword
 *
 * 	Architecture specific function to access kernel virtual
 *	address space.
 *
 * Parameters:
 *	None.
 * Returns:
 *	None.
 * Locking:
 *	None.
 * Remarks:
 *	None.
 */

/* 	if (access_ok(VERIFY_READ,__gu_addr,size))			\ */
 
extern inline void sync(void)
{
	asm volatile("sync; isync");
}

extern void (*debugger_fault_handler)(struct pt_regs *);
extern void longjmp(u_int *, int);
#if 0
static void handle_fault(struct pt_regs *);

static int fault_type;
#endif

unsigned long
kdba_getword(unsigned long addr, size_t width)
{
	/*
	 * This function checks the address for validity.  Any address
	 * in the range PAGE_OFFSET to high_memory is legal, any address
	 * which maps to a vmalloc region is legal, and any address which
	 * is a user address, we use get_user() to verify validity.
	 */

    if (!valid_ppc64_kernel_address(addr, width)) {
		        /*
			 * Would appear to be an illegal kernel address;
			 * Print a message once, and don't print again until
			 * a legal address is used.
			 */
			if (!KDB_STATE(SUPPRESS)) {
#if 1
				kdb_printf("    kdb: Possibly Bad kernel address 0x%lx \n",addr);
#else
				kdb_printf("kdb: ! \n");
#endif
				KDB_STATE_SET(SUPPRESS);
			}
			return 0L;
	}


	/*
	 * A good address.  Reset error flag.
	 */
	KDB_STATE_CLEAR(SUPPRESS);

	switch (width) {
	case 8:
	{	unsigned long *lp;

		lp = (unsigned long *)(addr);
		return *lp;
	}
	case 4:
	{	unsigned int *ip;

		ip = (unsigned int *)(addr);
		return *ip;
	}
	case 2:
	{	unsigned short *sp;

		sp = (unsigned short *)(addr);
		return *sp;
	}
	case 1:
	{	unsigned char *cp;

		cp = (unsigned char *)(addr);
		return *cp;
	}
	}

	kdb_printf("kdbgetword: Bad width\n");
	return 0L;
}



/*
 * kdba_putword
 *
 * 	Architecture specific function to access kernel virtual
 *	address space.
 *
 * Parameters:
 *	None.
 * Returns:
 *	None.
 * Locking:
 *	None.
 * Remarks:
 *	None.
 */

unsigned long
kdba_putword(unsigned long addr, size_t size, unsigned long contents)
{
	/*
	 * This function checks the address for validity.  Any address
	 * in the range PAGE_OFFSET to high_memory is legal, any address
	 * which maps to a vmalloc region is legal, and any address which
	 * is a user address, we use get_user() to verify validity.
	 */

	if (addr < PAGE_OFFSET) {
		/*
		 * Usermode address.
		 */
		unsigned long diag;

		switch (size) {
		case 4:
		{	unsigned long *lp;

			lp = (unsigned long *) addr;
			diag = put_user(contents, lp);
			break;
		}
		case 2:
		{	unsigned short *sp;

			sp = (unsigned short *) addr;
			diag = put_user(contents, sp);
			break;
		}
		case 1:
		{	unsigned char *cp;

			cp = (unsigned char *) addr;
			diag = put_user(contents, cp);
			break;
		}
		default:
			kdb_printf("kdba_putword: Bad width\n");
			return 0;
		}

		if (diag) {
			if (!KDB_STATE(SUPPRESS)) {
				kdb_printf("kdb: Bad user address 0x%lx\n", addr);
				KDB_STATE_SET(SUPPRESS);
			}
			return 0;
		}
		KDB_STATE_CLEAR(SUPPRESS);
		return 0;
	}

#if 0
	if (addr > (unsigned long)high_memory) {
		if (!kdb_vmlist_check(addr, addr+size)) {
			/*
			 * Would appear to be an illegal kernel address;
			 * Print a message once, and don't print again until
			 * a legal address is used.
			 */
			if (!KDB_STATE(SUPPRESS)) {
				kdb_printf("kdb: xx Bad kernel address 0x%lx\n", addr);
				KDB_STATE_SET(SUPPRESS);
			}
			return 0L;
		}
	}
#endif

	/*
	 * A good address.  Reset error flag.
	 */
	KDB_STATE_CLEAR(SUPPRESS);

	switch (size) {
	case 4:
	{	unsigned long *lp;

		lp = (unsigned long *)(addr);
		*lp = contents;
		return 0;
	}
	case 2:
	{	unsigned short *sp;

		sp = (unsigned short *)(addr);
		*sp = (unsigned short) contents;
		return 0;
	}
	case 1:
	{	unsigned char *cp;

		cp = (unsigned char *)(addr);
		*cp = (unsigned char) contents;
		return 0;
	}
	}

	kdb_printf("kdba_putword: Bad width\n");
	return 0;
}

/*
 * kdba_callback_die
 *
 *	Callback function for kernel 'die' function.
 *
 * Parameters:
 *	regs	Register contents at time of trap
 *	error_code  Trap-specific error code value
 *	trapno	Trap number
 *	vp	Pointer to die message
 * Returns:
 *	Returns 1 if fault handled by kdb.
 * Locking:
 *	None.
 * Remarks:
 *
 */
int
kdba_callback_die(struct pt_regs *regs, int error_code, long trapno, void *vp)
{
	/*
	 * Save a pointer to the message provided to 'die()'.
	 */
	kdb_diemsg = (char *)vp;

	return kdb(KDB_REASON_OOPS, error_code, (kdb_eframe_t) regs);
}

/*
 * kdba_callback_bp
 *
 *	Callback function for kernel breakpoint trap.
 *
 * Parameters:
 *	regs	Register contents at time of trap
 *	error_code  Trap-specific error code value
 *	trapno	Trap number
 *	vp	Not Used.
 * Returns:
 *	Returns 1 if fault handled by kdb.
 * Locking:
 *	None.
 * Remarks:
 *
 */

int
kdba_callback_bp(struct pt_regs *regs, int error_code, long trapno, void *vp)
{
	int diag;

	if (KDB_DEBUG(BP))
		kdb_printf("cb_bp: e_c = %d  tn = %ld regs = 0x%p\n", error_code,
			   trapno, regs);

	diag = kdb(KDB_REASON_BREAK, error_code, (kdb_eframe_t) regs);

	if (KDB_DEBUG(BP))
		kdb_printf("cb_bp: e_c = %d  tn = %ld regs = 0x%p diag = %d\n", error_code,
			   trapno, regs, diag);
	return diag;
}

/*
 * kdba_callback_debug
 *
 *	Callback function for kernel debug register trap.
 *
 * Parameters:
 *	regs	Register contents at time of trap
 *	error_code  Trap-specific error code value
 *	trapno	Trap number
 *	vp	Not used.
 * Returns:
 *	Returns 1 if fault handled by kdb.
 * Locking:
 *	None.
 * Remarks:
 *
 */

int
kdba_callback_debug(struct pt_regs *regs, int error_code, long trapno, void *vp)
{
	return kdb(KDB_REASON_DEBUG, error_code, (kdb_eframe_t) regs);
}




/*
 * kdba_adjust_ip
 *
 * 	Architecture specific adjustment of instruction pointer before leaving
 *	kdb.
 *
 * Parameters:
 *	reason		The reason KDB was invoked
 *	error		The hardware-defined error code
 *	ef		The exception frame at time of fault/breakpoint.  If reason
 *			is KDB_REASON_SILENT then ef is NULL, otherwise it should
 *			always be valid.
 * Returns:
 *	None.
 * Locking:
 *	None.
 * Remarks:
 *	noop on ix86.
 */

void
kdba_adjust_ip(kdb_reason_t reason, int error, kdb_eframe_t ef)
{
	return;
}



/*
 * kdba_find_tb_table
 *
 * 	Find the traceback table (defined by the ELF64 ABI) located at
 *	the end of the function containing pc.
 *
 * Parameters:
 *	eip	starting instruction addr.  does not need to be at the start of the func.
 *	tab	table to populate if successful
 * Returns:
 *	non-zero if successful.  unsuccessful means that a valid tb table was not found
 * Locking:
 *	None.
 * Remarks:
 *	None.
 */
int kdba_find_tb_table(kdb_machreg_t eip, kdbtbtable_t *tab)
{
	kdb_machreg_t codeaddr = eip;
	kdb_machreg_t codeaddr_max;
	kdb_machreg_t tbtab_start;
	int instr;
	int num_parms;

	if (tab == NULL)
		return 0;
	memset(tab, 0, sizeof(tab));

	if (eip < PAGE_OFFSET) {  /* this is gonna fail for userspace, at least for now.. */
	    return 0;
	}

	/* Scan instructions starting at codeaddr for 128k max */
	for (codeaddr_max = codeaddr + 128*1024*4;
	     codeaddr < codeaddr_max;
	     codeaddr += 4) {
		instr = kdba_getword(codeaddr, 4);
		if (instr == 0) {
			/* table should follow. */
			int version;
			unsigned long flags;
			tbtab_start = codeaddr;	/* save it to compute func start addr */
			codeaddr += 4;
			flags = kdba_getword(codeaddr, 8);
			tab->flags = flags;
			version = (flags >> 56) & 0xff;
			if (version != 0)
				continue;	/* No tb table here. */
			/* Now, like the version, some of the flags are values
			 that are more conveniently extracted... */
			tab->fp_saved = (flags >> 24) & 0x3f;
			tab->gpr_saved = (flags >> 16) & 0x3f;
			tab->fixedparms = (flags >> 8) & 0xff;
			tab->floatparms = (flags >> 1) & 0x7f;
			codeaddr += 8;
			num_parms = tab->fixedparms + tab->floatparms;
			if (num_parms) {
				unsigned int parminfo;
				int parm;
				if (num_parms > 32)
					return 1;	/* incomplete */
				parminfo = kdba_getword(codeaddr, 4);
				/* decode parminfo...32 bits.
				 A zero means fixed.  A one means float and the
				 following bit determines single (0) or double (1).
				 */
				for (parm = 0; parm < num_parms; parm++) {
					if (parminfo & 0x80000000) {
						parminfo <<= 1;
						if (parminfo & 0x80000000)
							tab->parminfo[parm] = KDBTBTAB_PARMDFLOAT;
						else
							tab->parminfo[parm] = KDBTBTAB_PARMSFLOAT;
					} else {
						tab->parminfo[parm] = KDBTBTAB_PARMFIXED;
					}
					parminfo <<= 1;
				}
				codeaddr += 4;
			}
			if (flags & KDBTBTAB_FLAGSHASTBOFF) {
				tab->tb_offset = kdba_getword(codeaddr, 4);
				if (tab->tb_offset > 0) {
					tab->funcstart = tbtab_start - tab->tb_offset;
				}
				codeaddr += 4;
			}
			/* hand_mask appears to be always be omitted. */
			if (flags & KDBTBTAB_FLAGSHASCTL) {
				/* Assume this will never happen for C or asm */
				return 1;	/* incomplete */
			}
			if (flags & KDBTBTAB_FLAGSNAMEPRESENT) {
				int i;
				short namlen = kdba_getword(codeaddr, 2);
				if (namlen >= sizeof(tab->name))
					namlen = sizeof(tab->name)-1;
				codeaddr += 2;
				for (i = 0; i < namlen; i++) {
					tab->name[i] = kdba_getword(codeaddr++, 1);
				}
				tab->name[namlen] = '\0';
			}
			/* Fake up a symtab entry in case the caller finds it useful */
			tab->symtab.value = tab->symtab.sym_start = tab->funcstart;
			tab->symtab.sym_name = tab->name;
			tab->symtab.sym_end = tbtab_start;
			return 1;
		}
	}
	return 0;	/* hit max...sorry. */
}


int
kdba_putarea_size(unsigned long to_xxx, void *from, size_t size)
{
    char c;
    kdb_printf("   ** this function calls copy_to_user...  \n");
    kdb_printf("   kdba_putarea_size [0x%ul]\n",(unsigned int) to_xxx);
    c = *((volatile char *)from);
    c=*((volatile char *)from+size-1);
    return __copy_to_user((void *)to_xxx,from,size);
}





/*
 * valid_ppc64_kernel_address() returns '1' if the address passed in is
 * within a valid range.  Function returns 0 if address is outside valid ranges.
 */

/*

    KERNELBASE    c000000000000000
        (good range)
    high_memory   c0000000 20000000

    VMALLOC_START d000000000000000
        (good range)
    VMALLOC_END   VMALLOC_START + VALID_EA_BITS  

    IMALLOC_START e000000000000000
        (good range)
    IMALLOC_END   IMALLOC_START + VALID_EA_BITS

*/

int valid_ppc64_kernel_address(unsigned long addr, unsigned long size)
{
	unsigned long i;
	unsigned long end = (addr + size - 1);


	for (i = addr; i <= end; i = i ++ ) {
	    if (((unsigned long)i < (unsigned long long)KERNELBASE     )  || 
		(((unsigned long)i > (unsigned long long)high_memory) &&
		 ((unsigned long)i < (unsigned long long)VMALLOC_START) )  ||
		(((unsigned long)i > (unsigned long long)VMALLOC_END) &&
		 ((unsigned long)i < (unsigned long long)IMALLOC_START) )  ||
		( (unsigned long)i > (unsigned long long)IMALLOC_END    )       ) {
		return 0;
	    }
	}
	return 1;
}


int
kdba_getarea_size(void *to, unsigned long from_xxx, size_t size)
{
	int is_valid_kern_addr = valid_ppc64_kernel_address(from_xxx, size);
	int diag = 0;

	*((volatile char *)to) = '\0';
	*((volatile char *)to + size - 1) = '\0';


	if (is_valid_kern_addr) {
		memcpy(to, (void *)from_xxx, size);
	} else {
            /*  user space address, just return.  */
	    diag = -1;
	}

	return diag;
}



/*
 *  kdba_readarea_size, reads size-lump of memory into to* passed in, returns size.
 * Making it feel a bit more like mread.. when i'm clearer on kdba end, probally will
 * remove one of these.
 */
int
kdba_readarea_size(unsigned long from_xxx,void *to, size_t size)
{
    int is_valid_kern_addr = valid_ppc64_kernel_address(from_xxx, size);

    *((volatile char *)to) = '\0';
    *((volatile char *)to + size - 1) = '\0';

    if (is_valid_kern_addr) {
	memcpy(to, (void *)from_xxx, size);
	return size;
    } else {
	/*  user-space, just return...    */
	return 0;
    }
    /* wont get here */
    return 0;
}

