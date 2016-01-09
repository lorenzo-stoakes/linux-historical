/*
 *  linux/include/linux/hfsplus_fs_sb.h
 *
 * Copyright (C) 1999
 * Brad Boyer (flar@pants.nu)
 *
 */

#ifndef _LINUX_HFSPLUS_FS_SB_H
#define _LINUX_HFSPLUS_FS_SB_H

#include <linux/types.h>

/*
 * HFS+ superblock info (built from Volume Header on disk)
 */

struct hfsplus_vh;
struct hfsplus_btree;

struct hfsplus_sb_info {
	struct buffer_head *s_vhbh;
	struct hfsplus_vh *s_vhdr;
	struct hfsplus_btree *ext_tree;
	struct hfsplus_btree *cat_tree;
	struct hfsplus_btree *attr_tree;
	struct inode *alloc_file;
	struct inode *hidden_dir;

	/* Runtime variables */
	u32 blockoffset;
	u32 sect_count;
	//int a2b_shift;

	/* Stuff in host order from Vol Header */
	u32 total_blocks;
	u32 free_blocks;
	u32 next_alloc;
	u32 next_cnid;
	u32 file_count;
	u32 folder_count;

	/* Config options */
	u32 creator;
	u32 type;

	int charcase;
	int fork;
	int namemap;

	umode_t umask;
	uid_t uid;
	gid_t gid;

	unsigned long flags;

	atomic_t inode_cnt;
	u32 last_inode_cnt;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
	struct list_head rsrc_inodes;
#else
	struct hlist_head rsrc_inodes;
#endif
};

#define HFSPLUS_SB_WRITEBACKUP	0x0001

#endif
