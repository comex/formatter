/*
	BootMii - a Free Software replacement for the Nintendo/BroadOn IOS.
	Requires mini.

	e-ticket stuff

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include "bootmii_ppc.h"
#include "ipc.h"
#include "mini_ipc.h"
#include "nandfs.h"
#include "string.h"
#include "sha1.h"
#include "es.h"

#define ASSERT(x) do { if(!(x)) { printf("ASSERT failure: %s in %s:%d\n", #x, __FILE__, __LINE__); return -1; } } while(0)

static char path[] = "/title/00000000/00000000/content/00000000.app";

static u32 get_uid(u64 titleid)
{
	// Create one in /sys/uid.sys if it doesn't exist

#if NANDFS_DUMMY
	return 0xdead; // Calling this more than once will screw up in dummy mode.
#endif

	struct uid temp __attribute__((aligned(32)));
	struct nandfs_fp fp;
	memset(&temp, 0, sizeof(temp));
	u16 maxuid = 0;
	u32 i;

	sprintf(path, "/sys/uid.sys");
	ASSERT(!nandfs_open(&fp, path));
	
	ASSERT(sizeof(temp) == 12);

	for(i = 0; i < fp.size; i += 12) {
		ASSERT(!nandfs_seek(&fp, i, NANDFS_SEEK_SET));
		ASSERT(sizeof(temp) == nandfs_read((u8*)&temp, sizeof(temp), 1, &fp));
		if(temp.uid > maxuid) maxuid = temp.uid;
		if(temp.title_id == titleid) {
			printf("get_uid: returning %d\n", temp.uid);
			return temp.uid;
		}
	}

	// Make a new one
	temp.title_id = titleid;
	temp.uid = maxuid + 1;
	ASSERT(!nandfs_seek(&fp, 0, NANDFS_SEEK_END));
	ASSERT(sizeof(temp) == nandfs_write((u8*)&temp, sizeof(temp), 1, &fp));

	return temp.uid;	
}

#if NANDFS_DUMMY
static u32 shared_i = 0;
#endif

static u32 get_shared_path(char *path, u8 hash[20])
{
#if NANDFS_DUMMY
	sprintf(path, "/shared1/%08x.app", shared_i++);
	return 0;
#endif
	struct nandfs_fp fp;
	struct content_map map __attribute__((aligned(32)));
	u32 num = 0xffffffff, temp;

	sprintf(path, "/shared1/content.map");
	ASSERT(!nandfs_open(&fp, path));
	while(sizeof(struct content_map) == nandfs_read(&map, sizeof(struct content_map), 1, &fp)) {
		temp = my_atoi_hex(map.path, 8);
		if(0 == memcmp(hash, map.hash, 20)) {
			// It already exists, no need to write it
			*path = 0;
			return 0;
		} else {
			if(num == 0xffffffff || temp > num) num = temp;
		}
	}
	// Make a new one
	num++;
	sprintf(map.path, "%08x", num);
	memcpy(map.hash, hash, 20);
	ASSERT(sizeof(struct content_map) == nandfs_write(&map, sizeof(struct content_map), 1, &fp));
	sprintf(path, "/shared1/%08x.app", num);
	printf("*** %s\n", path);
	return 0;
}

s32 es_addtitle(struct tmd *tmd, struct tik *tik)
{
	// Note: ASSERT failure leaves you with a possibly broken system
	// Welp!
	struct nandfs_fp fp;
	
	nandfs_initialize();

	ASSERT(tmd);
	ASSERT(tik);
	printf("Title ID: %llx vs %llx\n", tmd->title_id, tik->title_id);
	printf("TID offset: %x %x\n", ((u8 *) (&tmd->title_id)) - ((u8 *) (tmd)), ((u8 *) (&tik->title_id)) - ((u8 *) (tik)));
	ASSERT(tmd->title_id == tik->title_id);
	u32 tid_hi = tmd->title_id >> 32;
	u32 tid_lo = tmd->title_id & 0xffffffff;
	u32 uid = get_uid(tmd->title_id);
	ASSERT(uid != ((u32) -1));
	u16 gid = tmd->group_id;
	u32 tmd_size = 0x1e4 + 36 * tmd->num_contents;

	sprintf(path, "/title/%08x", tid_hi); 
	nandfs_create(path, 0, 0, NANDFS_ATTR_DIR, 3, 3, 1);

	sprintf(path, "/ticket/%08x", tid_hi); 
	nandfs_create(path, 0, 0, NANDFS_ATTR_DIR, 3, 3, 0);

	sprintf(path, "/title/%08x/%08x", tid_hi, tid_lo); 
	ASSERT(!nandfs_create(path, 0, 0, NANDFS_ATTR_DIR, 3, 3, 1));

	sprintf(path, "/title/%08x/%08x/content", tid_hi, tid_lo); 
	ASSERT(!nandfs_create(path, 0, 0, NANDFS_ATTR_DIR, 3, 3, 0));

	sprintf(path, "/title/%08x/%08x/data", tid_hi, tid_lo); 
	ASSERT(!nandfs_create(path, uid, gid, NANDFS_ATTR_DIR, 3, 0, 0));

	sprintf(path, "/title/%08x/%08x/content/title.tmd", tid_hi, tid_lo); 
	ASSERT(!nandfs_create(path, 0, 0, NANDFS_ATTR_FILE, 3, 3, 0));
	ASSERT(!nandfs_open(&fp, path));
	ASSERT(tmd_size == (u32) nandfs_write((void*)tmd, tmd_size, 1, &fp));

	sprintf(path, "/ticket/%08x/%08x.tik", tid_hi, tid_lo); 
	ASSERT(!nandfs_create(path, 0, 0, NANDFS_ATTR_FILE, 3, 3, 0));
	ASSERT(!nandfs_open(&fp, path));
	ASSERT(0x2a4 == nandfs_write((void*)tik, 0x2a4, 1, &fp));

	nandfs_writemeta();

	return 0;
}

s32 es_addtitlecontent(struct tmd *tmd, u16 index, void *buf, u32 size)
{
	u16 i;
	u32 tid_hi, tid_lo;
	SHA1Context ctx;
	struct nandfs_fp fp;

	nandfs_initialize();

	ASSERT(tmd);
	ASSERT(index < tmd->num_contents);
	ASSERT(buf);

	tid_hi = tmd->title_id >> 32;
	tid_lo = tmd->title_id & 0xffffffff;

	for(i = 0; i < tmd->num_contents; i++) {
		if(tmd->contents[i].index != index) continue;
		break;
	}

	ASSERT(i < tmd->num_contents);
	ASSERT(tmd->contents[i].size == size);
	SHA1Reset(&ctx);
	SHA1Input(&ctx, buf, size);
	ASSERT(SHA1Result(&ctx));
	if(memcmp(ctx.Message_Digest, tmd->contents[i].hash, 20)) {
		printf("Hash failure size=%d:\n", size);
		hexdump(ctx.Message_Digest, 20);
		hexdump(tmd->contents[i].hash, 20);
		if(size == 64) {
			hexdump(buf, 64);
		}
		while(1);
	}

	printf("cid %08x writing ", tmd->contents[i].cid);
	if(tmd->contents[i].type & 0x8000) {
		// shared content
		ASSERT(!get_shared_path(path, (u8 *) ctx.Message_Digest));
	} else {
		sprintf(path, "/title/%08x/%08x/content/%08x.app", tid_hi, tid_lo, tmd->contents[i].cid);
	}
	printf("PATH: %s %x %x %x\n", path, tid_hi, tid_lo, tmd->contents[i].cid);
	if(*path) {
		ASSERT(!nandfs_create(path, 0, 0, NANDFS_ATTR_FILE, 3, 3, 0));
		ASSERT(!nandfs_open(&fp, path));
		ASSERT(size == (u32) nandfs_write(buf, size, 1, &fp));
	}

	printf("done\n");

	nandfs_writemeta();

	return 0;
}

s32 es_format()
{
	nandfs_initialize();

	ASSERT(!nandfs_format());
	ASSERT(!nandfs_create("/shared1", 0, 0, NANDFS_ATTR_DIR, 3, 3, 0));
	ASSERT(!nandfs_create("/shared2", 0, 0, NANDFS_ATTR_DIR, 3, 3, 3));
	ASSERT(!nandfs_create("/shared2/sys", 0, 0, NANDFS_ATTR_DIR, 3, 3, 3));
	ASSERT(!nandfs_create("/sys", 0, 0, NANDFS_ATTR_DIR, 3, 3, 0));
	ASSERT(!nandfs_create("/tmp", 0, 0, NANDFS_ATTR_DIR, 3, 3, 3));
	ASSERT(!nandfs_create("/meta", 0, 0, NANDFS_ATTR_DIR, 3, 3, 3));
	ASSERT(!nandfs_create("/import", 0, 0, NANDFS_ATTR_DIR, 3, 3, 0));
	ASSERT(!nandfs_create("/title", 0, 0, NANDFS_ATTR_DIR, 3, 3, 1));
	ASSERT(!nandfs_create("/ticket", 0, 0, NANDFS_ATTR_DIR, 3, 3, 0));
	ASSERT(!nandfs_create("/shared1/content.map", 0, 0, NANDFS_ATTR_FILE, 3, 3, 0));
	ASSERT(!nandfs_create("/sys/uid.sys", 0, 0, NANDFS_ATTR_FILE, 3, 3, 0));

	nandfs_writemeta();
	return 0;
}
