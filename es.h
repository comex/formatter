/*
	BootMii - a Free Software replacement for the Nintendo/BroadOn IOS.
	Requires mini.

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#ifndef __ES_H__
#define __ES_H__

struct tmd_content {
	u32 cid;
	u16 index;
	u16 type;
	u64 size;
	u8 hash[20];
}  __attribute__((packed));

struct tmd {
	u8 stuff[0x180];
	u8 version;
	u8 ca_crl_version;
	u8 signer_crl_version;
	u8 fill2;
	u64 sys_version;
	u64 title_id;
	u32 title_type;
	u16 group_id;
	u16 zero;
	u16 region;
	u8 ratings[16];
	u8 reserved[12];
	u8 ipc_mask[12];
	u8 reserved2[18];
	u32 access_rights;
	u16 title_version;
	u16 num_contents;
	u16 boot_index;
	u16 fill3;
	// content records follow
	struct tmd_content contents[0];
} __attribute__((packed));

struct tiklimit {
	u32 tag;
	u32 value;
} __attribute__((packed));

struct tik {
	u8 stuff[0x180];
	u8 fill[63];
	u8 cipher_title_key[0x10];
	u8 fill2;
	u64 ticketid;
	u32 devicetype;
	u64 title_id;
	u16 access_mask;
	u8 reserved[0x3c];
	u8 cidx_mask[0x40];
	u16 padding;
	struct tiklimit limits[8];
} __attribute__((packed));

struct uid {
	u64 title_id;
	u16 padding;
	u16 uid;
} __attribute__((packed));

struct content_map {
	u8 path[8];
	u8 hash[20];
} __attribute__((packed));

s32 es_addtitle(struct tmd *tmd, struct tik *tik);
s32 es_addtitlecontent(struct tmd *tmd, u16 index, void *buf, u32 size);
s32 es_format();
#endif
