#ifndef _PPC64_STATFS_H
#define _PPC64_STATFS_H

/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#ifndef __KERNEL_STRICT_NAMES

#include <linux/types.h>

typedef __kernel_fsid_t	fsid_t;
typedef __kernel_fsid_t __kernel_fsid_t32;

#endif

/* 
 * Both SPARC64 & IA64 also define the following -
 */

struct statfs32 {
	int f_type;
	int f_bsize;
	int f_blocks;
	int f_bfree;
	int f_bavail;
	int f_files;
	int f_ffree;
	__kernel_fsid_t32 f_fsid;
	int f_namelen;  /* SunOS ignores this field. */
	int f_spare[6];
};

struct statfs {
	long f_type;
	long f_bsize;
	long f_blocks;
	long f_bfree;
	long f_bavail;
	long f_files;
	long f_ffree;
	__kernel_fsid_t f_fsid;
	long f_namelen;
	long f_spare[6];
};

#endif  /* _PPC64_STATFS_H */



