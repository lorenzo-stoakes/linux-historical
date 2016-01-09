#ifndef _PPC64_STAT_H
#define _PPC64_STAT_H

/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#include <linux/types.h>

struct stat32 {
	__kernel_dev_t32   st_dev; /* 2 */
	/* __kernel_dev_t32 __pad1; */ /* 2 */
	__kernel_ino_t32   st_ino; /* 4  */
	__kernel_mode_t32  st_mode; /* 2  */
	short   	   st_nlink; /* 2 */
	__kernel_uid_t32   st_uid; /* 2 */
	__kernel_gid_t32   st_gid; /* 2 */
	__kernel_dev_t32   st_rdev; /* 2 */
	/* __kernel_dev_t32 __pad2; */ /* 2 */
	__kernel_off_t32   st_size; /* 4 */
	__kernel_off_t32   st_blksize; /* 4 */
	__kernel_off_t32   st_blocks; /* 4 */
	__kernel_time_t32  st_atime; /* 4 */
	unsigned int       __unused1; /* 4 */
	__kernel_time_t32  st_mtime; /* 4 */
	unsigned int       __unused2; /* 4 */
	__kernel_time_t32  st_ctime; /* 4 */
	unsigned int       __unused3; /* 4 */
	unsigned int  __unused4[2]; /* 2*4 */
};

struct __old_kernel_stat {
	unsigned short st_dev;
	unsigned short st_ino;
	unsigned short st_mode;
	unsigned short st_nlink;
	unsigned short st_uid;
	unsigned short st_gid;
	unsigned short st_rdev;
	unsigned long  st_size;
	unsigned long  st_atime;
	unsigned long  st_mtime;
	unsigned long  st_ctime;
};

struct __old_kernel_stat32
{
	unsigned short st_dev;
	unsigned short st_ino;
	unsigned short st_mode;
	unsigned short st_nlink;
	unsigned short st_uid;
	unsigned short st_gid;
	unsigned short st_rdev;
	unsigned int   st_size;
	unsigned int   st_atime;
	unsigned int   st_mtime;
	unsigned int   st_ctime;
};

struct stat {
	dev_t		st_dev;
	ino_t		st_ino;
	mode_t		st_mode;
	nlink_t		st_nlink;
	uid_t 		st_uid;
	gid_t 		st_gid;
	dev_t		st_rdev;
	off_t		st_size;
	unsigned long  	st_blksize;
	unsigned long  	st_blocks;
	unsigned long  	st_atime;
	unsigned long  	__unused1;
	unsigned long  	st_mtime;
	unsigned long  	__unused2;
	unsigned long  	st_ctime;
	unsigned long  	__unused3;
	unsigned long  	__unused4;
	unsigned long  	__unused5;
};

/* This matches struct stat64 in glibc2.1. */
struct stat64 {
	unsigned long st_dev; 		/* Device.  */
	unsigned long st_ino;		/* File serial number.  */
	unsigned int st_mode;		/* File mode.  */
	unsigned int st_nlink;		/* Link count.  */
	unsigned int st_uid;		/* User ID of the file's owner.  */
	unsigned int st_gid;		/* Group ID of the file's group. */
	unsigned long st_rdev; 		/* Device number, if device.  */
	unsigned short __pad2;
	long st_size;			/* Size of file, in bytes.  */
	int  st_blksize;		/* Optimal block size for I/O.  */

	long st_blocks;			/* Number 512-byte blocks allocated. */
	int   st_atime;			/* Time of last access.  */
	unsigned int  __unused1;
	int   st_mtime;			/* Time of last modification.  */
	unsigned int  __unused2;
	int   st_ctime;			/* Time of last status change.  */
	unsigned int   __unused3;
	unsigned int   __unused4;
	unsigned int   __unused5;
};
#endif
