/*
 *  linux/include/linux/hfsplus_fs_i.h
 *
 * Copyright (C) 1999
 * Brad Boyer (flar@pants.nu)
 *
 */

#ifndef _LINUX_HFSPLUS_FS_I_H
#define _LINUX_HFSPLUS_FS_I_H

#include <linux/types.h>
#include <linux/version.h>
#include "hfsplus_raw.h"

struct hfsplus_inode_info {
	/* Device number in hfsplus_permissions in catalog */
	u32 dev;
	/* Allocation extents from catlog record or volume header */
	hfsplus_extent_rec extents;
	u32 total_blocks, extent_blocks, alloc_blocks;
	atomic_t opencnt;

	struct inode *rsrc_inode;
	unsigned long flags;

	struct list_head open_dir_list;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
	unsigned long mmu_private;
#else
	loff_t mmu_private;
	struct inode vfs_inode;
#endif
};

#define HFSPLUS_FLG_RSRC	0x0001

#define HFSPLUS_IS_DATA(inode)   (!(HFSPLUS_I(inode).flags & HFSPLUS_FLG_RSRC))
#define HFSPLUS_IS_RSRC(inode)   (HFSPLUS_I(inode).flags & HFSPLUS_FLG_RSRC)

#endif
