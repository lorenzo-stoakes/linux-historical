/*
 *  linux/fs/hfsplus/brec.c
 *
 * Copyright (C) 2001
 * Brad Boyer (flar@allandria.com)
 *
 * Handle individual btree records
 */

#include "hfsplus_fs.h"
#include "hfsplus_raw.h"

/* Get the offset of the given record in the given node */
u16 hfsplus_brec_off(hfsplus_bnode *node, u16 rec)
{
	u16 dataoff;

	dataoff = node->tree->node_size - (rec + 1) * 2;
	return hfsplus_bnode_read_u16(node, dataoff);
}

/* Get the length of the given record in the given node */
u16 hfsplus_brec_len(hfsplus_bnode *node, u16 rec)
{
	u16 retval[2];
	u16 dataoff;

	dataoff = node->tree->node_size - (rec + 2) * 2;
	hfsplus_bnode_readbytes(node, retval, dataoff, 4);
	return be16_to_cpu(retval[0]) - be16_to_cpu(retval[1]);
}

/* Get the length and offset of the given record in the given node */
u16 hfsplus_brec_lenoff(hfsplus_bnode *node, u16 rec, u16 *off)
{
	u16 retval[2];
	u16 dataoff;

	dataoff = node->tree->node_size - (rec + 2) * 2;
	hfsplus_bnode_readbytes(node, retval, dataoff, 4);
	*off = be16_to_cpu(retval[1]);
	return be16_to_cpu(retval[0]) - *off;
}

/* Copy a record from a node into a buffer, return the actual length */
u16 hfsplus_brec_data(hfsplus_bnode *node, u16 rec, char *buf,
			   u16 len)
{
	u16 recoff, reclen, cplen;

	reclen = hfsplus_brec_lenoff(node, rec, &recoff);
	if (!reclen)
		return 0;
	cplen = (reclen>len) ? len : reclen;
	hfsplus_bnode_readbytes(node, buf, recoff, cplen);
	return reclen;
}

/* Get the length of the key from a keyed record */
u16 hfsplus_brec_keylen(hfsplus_bnode *node, u16 rec)
{
	u16 klsz, retval, recoff;
	unsigned char buf[2];

	if ((node->kind != HFSPLUS_NODE_NDX)&&(node->kind != HFSPLUS_NODE_LEAF))
		return 0;

	klsz = (node->tree->attributes & HFSPLUS_TREE_BIGKEYS) ? 2 : 1;
	if ((node->kind == HFSPLUS_NODE_NDX) &&
	   !(node->tree->attributes & HFSPLUS_TREE_VAR_NDXKEY_SIZE)) {
		retval = node->tree->max_key_len;
	} else {
		recoff = hfsplus_brec_off(node, rec);
		if (!recoff)
			return 0;
		hfsplus_bnode_readbytes(node, buf, recoff, klsz);
		if (klsz == 1)
			retval = buf[0];
		else
			retval = be16_to_cpu(*(u16 *)buf);
	}
	return (retval + klsz + 1) & 0xFFFE;
}

/* Get a copy of the key of the given record, returns real key length */
u16 hfsplus_brec_key(hfsplus_bnode *node, u16 rec, void *buf,
			  u16 len)
{
	u16 recoff, reclen, keylen, tocopy;

	reclen = hfsplus_brec_lenoff(node, rec, &recoff);
	keylen = hfsplus_brec_keylen(node, rec);
	if (!reclen || !keylen)
		return 0;
	if (keylen > reclen) {
		printk("HFS+-fs: corrupt key length in B*Tree (%d,%d,%d,%d,%d)\n", node->this, rec, reclen, keylen, recoff);
		return 0;
	}
	tocopy = (len > keylen) ? keylen : len;
	hfsplus_bnode_readbytes(node, buf, recoff, tocopy);
	return keylen;
}
