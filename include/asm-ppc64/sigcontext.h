#ifndef _ASM_PPC64_SIGCONTEXT_H
#define _ASM_PPC64_SIGCONTEXT_H

/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#include <asm/ptrace.h>

struct sigcontext_struct {
	unsigned long	_unused[4];
	int		signal;
	unsigned long	handler;
	unsigned long	oldmask;
	struct pt_regs 	*regs;
};

#ifdef __KERNEL__

struct sigcontext32_struct {
	unsigned int	_unused[4];
	int		signal;
	unsigned int	handler;
	unsigned int	oldmask;
	u32 regs;  /* 4 byte pointer to the pt_regs32 structure. */
};

#endif /* __KERNEL__ */


#endif /* _ASM_PPC64_SIGCONTEXT_H */
