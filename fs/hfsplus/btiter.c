/*
 *  linux/fs/hfsplus/btiter.c
 *
 * Copyright (C) 2001
 * Brad Boyer (flar@allandria.com)
 *
 * Iterators for btrees
 */

#include "hfsplus_fs.h"

int hfsplus_btiter_move(struct hfsplus_find_data *fd, int cnt)
{
	struct hfsplus_btree *tree;
	hfsplus_bnode *bnode;
	int idx, res = 0;
	u16 off, len, keylen;

	bnode = fd->bnode;
	tree = bnode->tree;

	if (cnt < -0xFFFF || cnt > 0xFFFF)
		return -EINVAL;

	if (cnt < 0) {
		cnt = -cnt;
		while (cnt > fd->record) {
			cnt -= fd->record + 1;
			fd->record = bnode->num_recs - 1;
			idx = bnode->prev;
			if (!idx) {
				res = -ENOENT;
				goto out;
			}
			hfsplus_put_bnode(bnode);
			bnode = hfsplus_find_bnode(tree, idx);
			if (!bnode) {
				res = -EIO;
				goto out;
			}
		}
		fd->record -= cnt;
	} else {
		while (cnt >= bnode->num_recs - fd->record) {
			cnt -= bnode->num_recs - fd->record;
			fd->record = 0;
			idx = bnode->next;
			if (!idx) {
				res = -ENOENT;
				goto out;
			}
			hfsplus_put_bnode(bnode);
			bnode = hfsplus_find_bnode(tree, idx);
			if (!bnode) {
				res = -EIO;
				goto out;
			}
		}
		fd->record += cnt;
	}

	len = hfsplus_brec_lenoff(bnode, fd->record, &off);
	keylen = hfsplus_brec_keylen(bnode, fd->record);
	fd->keyoffset = off;
	fd->keylength = keylen;
	fd->entryoffset = off + keylen;
	fd->entrylength = len - keylen;
	hfsplus_bnode_readbytes(bnode, fd->key, off, keylen);
out:
	fd->bnode = bnode;
	return res;
}
