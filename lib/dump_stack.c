/*
 * Provide a default dump_stack() function for architectures
 * which don't implementtheir own.
 */

#include <linux/kernel.h>
#include <linux/module.h>

void dump_stack(void)
{
	printk(KERN_NOTICE
		"This architecture does not implement dump_stack()\n");
}
