/* utilities migrated from Xmon or other kernel debug tools. */

/*

Notes for migrating functions from xmon...
Add functions to this file.  parmlist for functions must match
   (int argc, const char **argv, const char **envp, struct pt_regs *fp)
add function prototype to kdbasupport.c
add function hook to kdba_init() within kdbasupport.c


Common bits...
mread() function calls need to be changed to kdba_readarea_size calls.  straightforward change.
This:
	nr = mread(codeaddr, &namlen, 2); 
becomes this:
	nr = kdba_readarea_size(codeaddr,&namlen,2);

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

#include "privinst.h"

#define EOF	(-1)

/* workaround for abort definition in kernel/pci.h */
#define abort(x) 
/* for traverse_all_pci_devices */
#include "../kernel/pci.h"
/* for NUM_TCE_LEVELS */
#include <asm/pci_dma.h>


/* prototypes */
int scanhex(unsigned long *);
int hexdigit(int c);
int kdba_readarea_size(unsigned long from_xxx,void *to,  size_t size);
void machine_halt(void); 



/*
 A traceback table typically follows each function.
 The find_tb_table() func will fill in this struct.  Note that the struct
 is not an exact match with the encoded table defined by the ABI.  It is
 defined here more for programming convenience.
 */
struct tbtable {
	unsigned long	flags;		/* flags: */
#define TBTAB_FLAGSGLOBALLINK	(1L<<47)
#define TBTAB_FLAGSISEPROL	(1L<<46)
#define TBTAB_FLAGSHASTBOFF	(1L<<45)
#define TBTAB_FLAGSINTPROC	(1L<<44)
#define TBTAB_FLAGSHASCTL	(1L<<43)
#define TBTAB_FLAGSTOCLESS	(1L<<42)
#define TBTAB_FLAGSFPPRESENT	(1L<<41)
#define TBTAB_FLAGSNAMEPRESENT	(1L<<38)
#define TBTAB_FLAGSUSESALLOCA	(1L<<37)
#define TBTAB_FLAGSSAVESCR	(1L<<33)
#define TBTAB_FLAGSSAVESLR	(1L<<32)
#define TBTAB_FLAGSSTORESBC	(1L<<31)
#define TBTAB_FLAGSFIXUP	(1L<<30)
#define TBTAB_FLAGSPARMSONSTK	(1L<<0)
	unsigned char	fp_saved;	/* num fp regs saved f(32-n)..f31 */
	unsigned char	gpr_saved;	/* num gpr's saved */
	unsigned char	fixedparms;	/* num fixed point parms */
	unsigned char	floatparms;	/* num float parms */
	unsigned char	parminfo[32];	/* types of args.  null terminated */
#define TBTAB_PARMFIXED 1
#define TBTAB_PARMSFLOAT 2
#define TBTAB_PARMDFLOAT 3
	unsigned int	tb_offset;	/* offset from start of func */
	unsigned long	funcstart;	/* addr of start of function */
	char		name[64];	/* name of function (null terminated)*/
};


static int find_tb_table(unsigned long codeaddr, struct tbtable *tab);


/* Very cheap human name for vector lookup. */
static
const char *getvecname(unsigned long vec)
{
	char *ret;
	switch (vec) {
	case 0x100:	ret = "(System Reset)"; break; 
	case 0x200:	ret = "(Machine Check)"; break; 
	case 0x300:	ret = "(Data Access)"; break; 
	case 0x400:	ret = "(Instruction Access)"; break; 
	case 0x500:	ret = "(Hardware Interrupt)"; break; 
	case 0x600:	ret = "(Alignment)"; break; 
	case 0x700:	ret = "(Program Check)"; break; 
	case 0x800:	ret = "(FPU Unavailable)"; break; 
	case 0x900:	ret = "(Decrementer)"; break; 
	case 0xc00:	ret = "(System Call)"; break; 
	case 0xd00:	ret = "(Single Step)"; break; 
	case 0xf00:	ret = "(Performance Monitor)"; break; 
	default: ret = "";
	}
	return ret;
}

int kdba_halt(int argc, const char **argv, const char **envp, struct pt_regs *fp)
{
    kdb_printf("halting machine. ");
    machine_halt();
return 0;
}


int kdba_excprint(int argc, const char **argv, const char **envp, struct pt_regs *fp)
{
	struct task_struct *c;
	struct tbtable tab;

#ifdef CONFIG_SMP
	kdb_printf("cpu %d: ", smp_processor_id());
#endif /* CONFIG_SMP */

	kdb_printf("Vector: %lx %s at  [%p]\n", fp->trap, getvecname(fp->trap), fp);
	kdb_printf("    pc: %lx", fp->nip);
	if (find_tb_table(fp->nip, &tab) && tab.name[0]) {
		/* Got a nice name for it */
		int delta = fp->nip - tab.funcstart;
		kdb_printf(" (%s+0x%x)", tab.name, delta);
	}
	kdb_printf("\n");
	kdb_printf("    lr: %lx", fp->link);
	if (find_tb_table(fp->link, &tab) && tab.name[0]) {
		/* Got a nice name for it */
		int delta = fp->link - tab.funcstart;
		kdb_printf(" (%s+0x%x)", tab.name, delta);
	}
	kdb_printf("\n");
	kdb_printf("    sp: %lx\n", fp->gpr[1]);
	kdb_printf("   msr: %lx\n", fp->msr);

	if (fp->trap == 0x300 || fp->trap == 0x600) {
		kdb_printf("   dar: %lx\n", fp->dar);
		kdb_printf(" dsisr: %lx\n", fp->dsisr);
	}

	/* XXX: need to copy current or we die.  Why? */
	c = current;
	kdb_printf("  current = 0x%p\n", c);
	kdb_printf("  paca    = 0x%p\n", get_paca());
	if (c) {
		kdb_printf("  current = %p, pid = %ld, comm = %s\n",
		       c, (unsigned long)c->pid, (char *)c->comm);
	}
return 0;
}




/* Starting at codeaddr scan forward for a tbtable and fill in the
 given table.  Return non-zero if successful at doing something.
 */
static int
find_tb_table(unsigned long codeaddr, struct tbtable *tab)
{
	unsigned long codeaddr_max;
	unsigned long tbtab_start;
	int nr;
	int instr;
	int num_parms;

	if (tab == NULL)
		return 0;
	memset(tab, 0, sizeof(tab));

	/* Scan instructions starting at codeaddr for 128k max */
	for (codeaddr_max = codeaddr + 128*1024*4;
	     codeaddr < codeaddr_max;
	     codeaddr += 4) {
	    nr=kdba_readarea_size(codeaddr,&instr,4);
		if (nr != 4)
			return 0;	/* Bad read.  Give up promptly. */
		if (instr == 0) {
			/* table should follow. */
			int version;
			unsigned long flags;
			tbtab_start = codeaddr;	/* save it to compute func start addr */
			codeaddr += 4;
			nr = kdba_readarea_size(codeaddr,&flags,8);
			if (nr != 8)
				return 0;	/* Bad read or no tb table. */
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
				nr = kdba_readarea_size(codeaddr,&parminfo,4);
				if (nr != 4)
					return 1;	/* incomplete */
				/* decode parminfo...32 bits.
				 A zero means fixed.  A one means float and the
				 following bit determines single (0) or double (1).
				 */
				for (parm = 0; parm < num_parms; parm++) {
					if (parminfo & 0x80000000) {
						parminfo <<= 1;
						if (parminfo & 0x80000000)
							tab->parminfo[parm] = TBTAB_PARMDFLOAT;
						else
							tab->parminfo[parm] = TBTAB_PARMSFLOAT;
					} else {
						tab->parminfo[parm] = TBTAB_PARMFIXED;
					}
					parminfo <<= 1;
				}
				codeaddr += 4;
			}
			if (flags & TBTAB_FLAGSHASTBOFF) {
			    nr = kdba_readarea_size(codeaddr,&tab->tb_offset,4);
				if (nr != 4)
					return 1;	/* incomplete */
				if (tab->tb_offset > 0) {
					tab->funcstart = tbtab_start - tab->tb_offset;
				}
				codeaddr += 4;
			}
			/* hand_mask appears to be always be omitted. */
			if (flags & TBTAB_FLAGSHASCTL) {
				/* Assume this will never happen for C or asm */
				return 1;	/* incomplete */
			}
			if (flags & TBTAB_FLAGSNAMEPRESENT) {
				short namlen;
				nr = kdba_readarea_size(codeaddr,&namlen,2);
				if (nr != 2)
					return 1;	/* incomplete */
				if (namlen >= sizeof(tab->name))
					namlen = sizeof(tab->name)-1;
				codeaddr += 2;
				nr = kdba_readarea_size(codeaddr,tab->name,namlen);
				tab->name[namlen] = '\0';
				codeaddr += namlen;
			}
			return 1;
		}
	}
	return 0;	/* hit max...sorry. */
}


int kdba_dissect_msr(int argc, const char **argv, const char **envp, struct pt_regs *regs)
{
   long int msr;
   msr = get_msr();

   kdb_printf("msr dissection: %lx\n",msr);
   if (msr & MSR_SF)   kdb_printf(" 64 bit mode enabled \n");
   if (msr & MSR_ISF)  kdb_printf(" Interrupt 64b mode valid on 630 \n");
   if (msr & MSR_HV)   kdb_printf(" Hypervisor State \n");
   if (msr & MSR_VEC)  kdb_printf(" Enable Altivec \n");
   if (msr & MSR_POW)  kdb_printf(" Enable Power Management  \n");
   if (msr & MSR_WE)   kdb_printf(" Wait State Enable   \n");
   if (msr & MSR_TGPR) kdb_printf(" TLB Update registers in use   \n");
   if (msr & MSR_CE)   kdb_printf(" Critical Interrupt Enable   \n");
   if (msr & MSR_ILE)  kdb_printf(" Interrupt Little Endian   \n");
   if (msr & MSR_EE)   kdb_printf(" External Interrupt Enable   \n");
   if (msr & MSR_PR)   kdb_printf(" Problem State / Privilege Level  \n"); 
   if (msr & MSR_FP)   kdb_printf(" Floating Point enable   \n");
   if (msr & MSR_ME)   kdb_printf(" Machine Check Enable   \n");
   if (msr & MSR_FE0)  kdb_printf(" Floating Exception mode 0  \n"); 
   if (msr & MSR_SE)   kdb_printf(" Single Step   \n");
   if (msr & MSR_BE)   kdb_printf(" Branch Trace   \n");
   if (msr & MSR_DE)   kdb_printf(" Debug Exception Enable   \n");
   if (msr & MSR_FE1)  kdb_printf(" Floating Exception mode 1   \n");
   if (msr & MSR_IP)   kdb_printf(" Exception prefix 0x000/0xFFF   \n");
   if (msr & MSR_IR)   kdb_printf(" Instruction Relocate   \n");
   if (msr & MSR_DR)   kdb_printf(" Data Relocate   \n");
   if (msr & MSR_PE)   kdb_printf(" Protection Enable   \n");
   if (msr & MSR_PX)   kdb_printf(" Protection Exclusive Mode   \n");
   if (msr & MSR_RI)   kdb_printf(" Recoverable Exception   \n");
   if (msr & MSR_LE)   kdb_printf(" Little Endian   \n");
   kdb_printf(".\n");
return 0;
}





int kdba_super_regs(int argc, const char **argv, const char **envp, struct pt_regs *regs){
	int i;
/*	int cmd; */
/*	unsigned long val; */
	struct paca_struct*  ptrPaca = NULL;
	struct ItLpPaca*  ptrLpPaca = NULL;
	struct ItLpRegSave*  ptrLpRegSave = NULL;

/*	cmd = skipbl(); */
/*	if (cmd == '\n') { */
	{
	        unsigned long sp, toc;
		kdb_printf("sr::");
		asm("mr %0,1" : "=r" (sp) :);
		asm("mr %0,2" : "=r" (toc) :);

		kdb_printf("msr  = %.16lx  sprg0= %.16lx\n", get_msr(), get_sprg0());
		kdb_printf("pvr  = %.16lx  sprg1= %.16lx\n", get_pvr(), get_sprg1()); 
		kdb_printf("dec  = %.16lx  sprg2= %.16lx\n", get_dec(), get_sprg2());
		kdb_printf("sp   = %.16lx  sprg3= %.16lx\n", sp, get_sprg3());
		kdb_printf("toc  = %.16lx  dar  = %.16lx\n", toc, get_dar());
		kdb_printf("srr0 = %.16lx  srr1 = %.16lx\n", get_srr0(), get_srr1());
		kdb_printf("asr  = %.16lx\n", mfasr());
		for (i = 0; i < 8; ++i)
			kdb_printf("sr%.2ld = %.16lx  sr%.2ld = %.16lx\n", (long int)i, (unsigned long)get_sr(i), (long int)(i+8), (long unsigned int) get_sr(i+8));

		// Dump out relevant Paca data areas.
		kdb_printf("Paca: \n");
		ptrPaca = (struct paca_struct*)get_sprg3();
    
		kdb_printf("  Local Processor Control Area (LpPaca): \n");
		ptrLpPaca = ptrPaca->xLpPacaPtr;
		kdb_printf("    Saved Srr0=%.16lx  Saved Srr1=%.16lx \n", ptrLpPaca->xSavedSrr0, ptrLpPaca->xSavedSrr1);
		kdb_printf("    Saved Gpr3=%.16lx  Saved Gpr4=%.16lx \n", ptrLpPaca->xSavedGpr3, ptrLpPaca->xSavedGpr4);
		kdb_printf("    Saved Gpr5=%.16lx \n", ptrLpPaca->xSavedGpr5);
    
		kdb_printf("  Local Processor Register Save Area (LpRegSave): \n");
		ptrLpRegSave = ptrPaca->xLpRegSavePtr;
		kdb_printf("    Saved Sprg0=%.16lx  Saved Sprg1=%.16lx \n", ptrLpRegSave->xSPRG0, ptrLpRegSave->xSPRG0);
		kdb_printf("    Saved Sprg2=%.16lx  Saved Sprg3=%.16lx \n", ptrLpRegSave->xSPRG2, ptrLpRegSave->xSPRG3);
		kdb_printf("    Saved Msr  =%.16lx  Saved Nia  =%.16lx \n", ptrLpRegSave->xMSR, ptrLpRegSave->xNIA);
    
		return 0;
	} 


}



	
int kdba_dump_tce_table(int argc, const char **argv, const char **envp, struct pt_regs *regs){
    struct TceTable kt; 
    long tce_table_address;
    int nr;
    int i,j,k;
    int full,empty;
    int fulldump=0;
    u64 mapentry;
    int totalpages;
    int levelpages;

    if (argc == 0) {
	kdb_printf("need address\n");
	return 0;
    }
    else 
	kdbgetularg(argv[1], &tce_table_address);

    if (argc==2)
	if (strcmp(argv[2], "full") == 0) 
	    fulldump=1;


    /* with address, read contents of memory and dump tce table. */
    /* possibly making some assumptions on the depth and size of table..*/

    nr = kdba_readarea_size(tce_table_address+0 ,&kt.busNumber,8);
    nr = kdba_readarea_size(tce_table_address+8 ,&kt.size,8);
    nr = kdba_readarea_size(tce_table_address+16,&kt.startOffset,8);
    nr = kdba_readarea_size(tce_table_address+24,&kt.base,8);
    nr = kdba_readarea_size(tce_table_address+32,&kt.index,8);
    nr = kdba_readarea_size(tce_table_address+40,&kt.tceType,8);
    nr = kdba_readarea_size(tce_table_address+48,&kt.lock,8);

    kdb_printf("\n");
    kdb_printf("TceTable at address %s:\n",argv[1]);
    kdb_printf("BusNumber:   0x%x \n",(uint)kt.busNumber);
    kdb_printf("size:        0x%x \n",(uint)kt.size);
    kdb_printf("startOffset: 0x%x \n",(uint)kt.startOffset);
    kdb_printf("base:        0x%x \n",(uint)kt.base);
    kdb_printf("index:       0x%x \n",(uint)kt.index);
    kdb_printf("tceType:     0x%x \n",(uint)kt.tceType);
    kdb_printf("lock:        0x%x \n",(uint)kt.lock.lock);

    nr = kdba_readarea_size(tce_table_address+56,&kt.mlbm.maxLevel,8);
    kdb_printf(" maxLevel:        0x%x \n",(uint)kt.mlbm.maxLevel);
    totalpages=0;
    for (i=0;i<NUM_TCE_LEVELS;i++) {
	nr = kdba_readarea_size(tce_table_address+64+i*24,&kt.mlbm.level[i].numBits,8);
	nr = kdba_readarea_size(tce_table_address+72+i*24,&kt.mlbm.level[i].numBytes,8);
	nr = kdba_readarea_size(tce_table_address+80+i*24,&kt.mlbm.level[i].map,8);
	kdb_printf("   level[%d]\n",i);
	kdb_printf("   numBits:   0x%x\n",(uint)kt.mlbm.level[i].numBits);
	kdb_printf("   numBytes:  0x%x\n",(uint)kt.mlbm.level[i].numBytes);
	kdb_printf("   map*:      %p\n",kt.mlbm.level[i].map);

	 /* if these dont match, this might not be a valid tce table, so
	    dont try to iterate the map entries. */
	if (kt.mlbm.level[i].numBits == 8*kt.mlbm.level[i].numBytes) {
	    full=0;empty=0;levelpages=0;
	    for (j=0;j<kt.mlbm.level[i].numBytes; j++) {
		mapentry=0;
		nr = kdba_readarea_size((long int)(kt.mlbm.level[i].map+j),&mapentry,1);
		if (mapentry)
		    full++;
		else
		    empty++;
		if (mapentry && fulldump) {
		    kdb_printf("0x%lx\n",mapentry);
		}
		for (k=0;(k<=64) && ((0x1UL<<k) <= mapentry);k++) {
		    if ((0x1UL<<k) & mapentry) levelpages++;
		}
	    }
	    kdb_printf("      full:0x%x empty:0x%x pages:0x%x\n",full,empty,levelpages);
	} else {
	    kdb_printf("      numBits/numBytes mismatch..? \n");
	}
	totalpages+=levelpages;
    }
    kdb_printf("      Total pages:0x%x\n",totalpages);
    kdb_printf("\n");
    return 0;
}

int kdba_kernelversion(int argc, const char **argv, const char **envp, struct pt_regs *regs){
    kdb_symtab_t   symtab;
    long banner_start_addr;
    char * banner_addr;
    int nr;

    banner_start_addr = kdbgetsymval("linux_banner", &symtab);
    if (banner_start_addr) {
	banner_start_addr = symtab.sym_start;
    } else {
	kdb_printf("linux_banner symbol not found! \n");
	return 0;
    }

    nr = kdba_readarea_size(banner_start_addr,&banner_addr,8);

    kdb_printf("%s",banner_addr);

    return 0;
}

int kdba_dmesg(int argc, const char **argv, const char **envp, struct pt_regs *regs){
    kdb_symtab_t   symtab;
    long log_buf_addr=0;
    long log_start_addr=0;
    int nr;                 
    unsigned long default_lines;   /* number of lines to read */
    long index_into_log_buf; /* pointer into log_buf */
    long saved_index;  /* saved pointer into log_buf */
    char current_char; /* temp char value */
    int line_count=0;  /* temp counter for # lines*/
    long log_size;  /* size of log_buf */
    int wrapped; 

    if (argc == 0)
	default_lines=10; 
    else 
	kdbgetularg(argv[1], &default_lines);

    log_buf_addr = kdbgetsymval("log_buf", &symtab);
    if (log_buf_addr) {
	log_buf_addr = symtab.sym_start;
    } else {
	kdb_printf("log_buf symbol not found! Can't do dmesg.\n");
	return 0;
    }
    log_start_addr = kdbgetsymval("log_start", &symtab);
    if (log_start_addr) {
	log_start_addr = symtab.sym_start;
    } else {
	kdb_printf("log_start symbol not found! Can't do dmesg.\n");
	return 0;
    }

    log_size = log_start_addr - log_buf_addr;

    nr = kdba_readarea_size(log_start_addr,&index_into_log_buf,8);

    saved_index=index_into_log_buf;
    if (index_into_log_buf > log_size ) {
	wrapped=1;
    } else {
	wrapped=0;
    }

    while ((index_into_log_buf > 0 ) && (line_count <= default_lines)) {
	nr = kdba_readarea_size(log_buf_addr+(index_into_log_buf%log_size),&current_char,1);
	if (current_char == 0x0a ) {
	    line_count++;
	}	
	index_into_log_buf--;
    }

    if (line_count < default_lines ) {
	kdb_printf("Something went wrong trying to count %ld lines\n",default_lines);
    }

    while ((index_into_log_buf < saved_index+1) && line_count >= 0 ) {
	nr = kdba_readarea_size(log_buf_addr+index_into_log_buf%log_size,&current_char,1);
	kdb_printf("%c",current_char);
	if (current_char == 0x0a) line_count--;
	index_into_log_buf++;
    }

    return 0; 
}


static void * 
kdba_dump_pci(struct device_node *dn, void *data)
{
    struct pci_controller *phb;
    char *device_type;
    char *status;

    phb = (struct pci_controller *)data;
    device_type = get_property(dn, "device_type", 0);
    status = get_property(dn, "status", 0);

    dn->phb = phb;
    kdb_printf("dn:   %p \n",dn);
    kdb_printf("    phb      : %p\n",dn->phb);
    kdb_printf("    name     : %s\n",dn->name);
    kdb_printf("    full_name: %s\n",dn->full_name);
    kdb_printf("    busno    : 0x%x\n",dn->busno);
    kdb_printf("    devfn    : 0x%x\n",dn->devfn);
    kdb_printf("    tce_table: %p\n",dn->tce_table);
    return NULL;
}


int kdba_dump_pci_info(int argc, const char **argv, const char **envp, struct pt_regs *regs){

    kdb_printf("kdba_dump_pci_info\n");

/* call this traverse function with my function pointer.. it takes care of traversing, my func just needs to parse the device info.. */
    traverse_all_pci_devices(kdba_dump_pci);
    return 0;
}





