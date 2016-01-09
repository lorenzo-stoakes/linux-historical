#ifndef _ASMPPC64_UCONTEXT_H
#define _ASMPPC64_UCONTEXT_H

/* Copied from i386. 
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

struct ucontext {
	unsigned long	  uc_flags;
	struct ucontext  *uc_link;
	stack_t		  uc_stack;
	struct sigcontext_struct uc_mcontext;
	sigset_t	  uc_sigmask;	/* mask last for extensibility */
};

#ifdef __KERNEL__


struct ucontext32 { 
	unsigned int	  uc_flags;
	unsigned int 	  uc_link;
	stack_32_t	  uc_stack;
	struct sigcontext32_struct uc_mcontext;
	sigset_t	  uc_sigmask;	/* mask last for extensibility */
};

#endif /* __KERNEL__ */


#endif /* _ASMPPC64_UCONTEXT_H */
