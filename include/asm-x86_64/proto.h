#ifndef _ASM_X8664_PROTO_H
#define _ASM_X8664_PROTO_H 1

/* misc architecture specific prototypes */

struct cpuinfo_x86; 

extern void get_cpu_vendor(struct cpuinfo_x86*);
extern void start_kernel(void);
extern void pda_init(int); 

extern void mcheck_init(struct cpuinfo_x86 *c);
extern void init_memory_mapping(void);

extern void system_call(void); 
extern void ia32_cstar_target(void); 
extern void calibrate_delay(void);
extern void cpu_idle(void);
extern void sys_ni_syscall(void);
extern void config_acpi_tables(void);
extern void ia32_syscall(void);

extern void do_softirq_thunk(void);

extern int setup_early_printk(char *); 
extern void early_printk(const char *fmt, ...) __attribute__((format(printf,1,2)));

#endif
