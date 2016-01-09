/*
 * Branch and jump emulation.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1996, 1997, 1998, 2001 by Ralf Baechle
 */
#include <asm/ptrace.h>

static inline int delay_slot(struct pt_regs *regs)
{
	return regs->cp0_cause & CAUSEF_BD;
}

extern int __compute_return_epc(struct pt_regs *regs);

static inline int compute_return_epc(struct pt_regs *regs)
{
	if (!delay_slot(regs)) {
		regs->cp0_epc += 4;
		return 0;
	}

	return __compute_return_epc(regs);
}
