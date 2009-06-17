/*
	BootMii - a Free Software replacement for the Nintendo/BroadOn bootloader.
	Requires mini.

Copyright (C) 2008, 2009	Sven Peter <svenpeter@gmail.com>

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#ifndef __NANDFS_H__
#define __NANDFS_H__

#define	NANDFS_NAME_LEN	12

#define	NANDFS_SEEK_SET	0
#define	NANDFS_SEEK_CUR	1
#define	NANDFS_SEEK_END	2

#define NANDFS_LAST_SIBLING 0
#define NANDFS_LAST_CHILD 1

#define NANDFS_DUMMY 0
#define NANDFS_VERBOSE NAND_VERBOSE

#define NANDFS_ATTR_FILE		1
#define NANDFS_ATTR_DIR			2

struct nandfs_fp {
	u16 first_cluster;
	u16 cur_cluster;
	short cluster_idx;
	u32 size;
	u32 offset;
	struct _nandfs_file_node *node;
	u32 idx;
	// Info for deletion.
	struct _nandfs_file_node *last;
	u8 last_type;
};

s32 nandfs_initialize(void);
u32 nandfs_get_usage(void);
s32 nandfs_walk();
s32 nandfs_format();
s32 nandfs_create(const char *path, u32 uid, u16 gid, u8 attr, u8 user_perm, u8 group_perm, u8 other_perm);
s32 nandfs_open(struct nandfs_fp *fp, const char *path);
s32 nandfs_read(u8 *ptr, u32 size, u32 nmemb, struct nandfs_fp *fp);
s32 nandfs_write(const u8 *ptr, u32 size, u32 nmemb, struct nandfs_fp *fp);
s32 nandfs_seek(struct nandfs_fp *fp, s32 offset, u32 whence);
s32 nandfs_delete(struct nandfs_fp *fp);
#endif

