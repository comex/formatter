/*
	BootMii - a Free Software replacement for the Nintendo/BroadOn IOS.
	Requires mini.

	NAND filesystem support

Copyright (C) 2008, 2009	Sven Peter <svenpeter@gmail.com>

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include "bootmii_ppc.h"
#include "ipc.h"
#include "mini_ipc.h"
#include "nandfs.h"
#include "fs_hmac.h"
#include "string.h"
#include "otp.h"
#include "aes.h"

#define	PAGE_SIZE	2048
#define NANDFS_CLUSTER_LAST		0xFFFB // cluster_map[last cluster in
                                       // the file] is this, as well as
                                       // an empty file's first_cluster
#define NANDFS_CLUSTER_HDR		0xFFFC // Part of the header so don't use it
#define NANDFS_CLUSTER_BAD		0xFFFD
#define NANDFS_CLUSTER_FREE		0xFFFE
#define NANDFS_CLUSTER_UNK		0xFFFF // I have some of these but 
                                       // sure what they are

struct _nandfs_file_node {
	char name[NANDFS_NAME_LEN];
	u8 attr;
	u8 wtf;
	union {
		u16 first_child;
		u16 first_cluster;
	};
	u16 sibling;
	u32 size;
	u32 uid;
	u16 gid;
	u32 dummy;
} __attribute__((packed));

struct _nandfs_sffs {
	u8 magic[4];
	u32 version;
	u32 dummy;

	u16 cluster_table[32768];
	struct _nandfs_file_node files[6143];
} __attribute__((packed));


union _sffs_t {
	u8 buffer[16*8*2048];
	struct _nandfs_sffs sffs;
};

static union _sffs_t sffs __attribute__((aligned(32)));
static u32 supercluster = 0;

/*static u8 _sffs_buffer[16*8*2048] __attribute__((aligned(32)));
static struct _nandfs_sffs *sffs = (struct _nandfs_sffs *)&_sffs_buffer;*/
static u8 buffer[8*2048] __attribute__((aligned(128)));
static u8 buffer2[2048] __attribute__((aligned(128)));
static u8 blockdata[64][2048+0x40] __attribute__((aligned(128)));
static u32 blockno = (u32) -1;

static s32 initialized = 0;

static u8 parity(u8 x) {
	u8 y = 0;

	while (x) {
		y ^= (x & 1);
		x >>= 1;
	}

	return y;
}

void nand_calc_ecc(const u8 *data, u8 *ecc) {
	int i, j, k;
	u8 a[12][2];
	u32 a0, a1;
	u8 x;

	for (k = 0; k < 4; k++) {
		memset(a, 0, sizeof a);
		for (i = 0; i < 512; i++) {
			x = data[i];
			for (j = 0; j < 9; j++)
				a[3+j][(i >> j) & 1] ^= x;
		}

		x = a[3][0] ^ a[3][1];
		a[0][0] = x & 0x55;
		a[0][1] = x & 0xaa;
		a[1][0] = x & 0x33;
		a[1][1] = x & 0xcc;
		a[2][0] = x & 0x0f;
		a[2][1] = x & 0xf0;

		for (j = 0; j < 12; j++) {
			a[j][0] = parity(a[j][0]);
			a[j][1] = parity(a[j][1]);
		}

		a0 = a1 = 0;
		for (j = 0; j < 12; j++) {
			a0 |= a[j][0] << j;
			a1 |= a[j][1] << j;
		}

		ecc[0] = a0;
		ecc[1] = a0 >> 8;
		ecc[2] = a1;
		ecc[3] = a1 >> 8;

		data += 512;
		ecc += 4;
	}
}

static void nand_lazy_write(u32 pageno, void *data, void *ecc)
{
	u32 myblock = pageno / 64;
	u32 p;
#if NANDFS_DUMMY
#if NANDFS_VERBOSE >= 2
	printf("NOT lazy writing to page %x\n", pageno);
#endif
	return;
#else
#if NANDFS_VERBOSE >= 2
	printf("Lazy writing to page %x\n", pageno);
#endif
#endif
#if NANDFS_VERBOSE >= 3
	if(data) hexdump(data, 4);
	if(ecc) {
		hexdump(ecc, 0x40);
	}
#endif
	if(myblock != blockno) {
#ifdef NANDFS_VERBOSE >= 2
		printf("Switching from block %x to block %x\n", blockno, myblock);
#endif
		if(blockno != ((u32) -1)) {
			// Commit the previous changes.
			nand_erase(64 * blockno);
			for(p = 0; p < 64; p++) {
				nand_write(64 * blockno + p, blockdata[p], blockdata[p] + 2048);
			}
		}
		blockno = pageno == ((u32) -1) ? ((u32) -1) : myblock;
		if(pageno != ((u32) -1)) {
			// Load.
			for(p = 0; p < 64; p++) {
				nand_read(64 * blockno + p, blockdata[p], blockdata[p] + 2048);
			}
		}
	}
	p = pageno % 64;
	if(data)
		memcpy(blockdata[p], data, 2048);
	if(ecc)
		memcpy(blockdata[p] + 2048, ecc, 0x40);
}

void nand_read_cluster(u32 pageno, u8 *buffer)
{
	int i;
	for (i = 0; i < 8; i++)
		nand_read(pageno + i, buffer + (i * PAGE_SIZE), NULL);
}

void nand_write_cluster(u32 pageno, u8 *buffer, u8 *hmac) {
	// Thisfunction is a clusterfuck.
	// Calculate the ECC.
		
	int i;
	for (i = 0; i < 8; i++) {
		static u8 spare[64] __attribute__((aligned(128)));
#if NANDFS_VERBOSE >= 2
		printf("hmac: %08x	spare: %08x\n", hmac, spare);
#endif
		nand_read(pageno + i, buffer2, spare);
		nand_calc_ecc(buffer + (i * PAGE_SIZE), spare + 0x30);
		spare[0] = 0xff; // good block
		if(hmac) {
			if(i == 6) {
				memcpy(spare + 1, hmac, 20);
				memcpy(spare + 21, hmac, 12);
			} else if(i == 7) {
				memcpy(spare + 1, hmac + 12, 8);
			}
		}
		nand_lazy_write(pageno + i, buffer + (i * PAGE_SIZE), spare);
	}
}

void nand_read_decrypted_cluster(u32 pageno, u8 *buffer)
{
	static u8 iv[16] __attribute__((aligned(32))) = {0,};
	nand_read_cluster(pageno, buffer);

	aes_reset();
	aes_set_iv(iv);
	aes_set_key(otp.nand_key);
	aes_decrypt(buffer, buffer, 0x400, 0);
}

void nand_write_decrypted_cluster(u32 pageno, u8 *buffer, struct nandfs_fp *fp)
{
	static u8 iv[16] __attribute__((aligned(32))) = {0,};

	static u8 hmac[20] __attribute__((aligned(128)));
	memset(hmac, 0xba, 20);
#if NANDFS_VERBOSE >= 3
	printf("uid: %d name: %s idx: %d dummy: %d cluster_idx: %d\n", fp->node->uid, fp->node->name, fp->idx, fp->node->dummy, fp->cluster_idx);
	hexdump(fp->node->name, 12);
	printf("nand_hmac:\n");
	hexdump(otp.nand_hmac, 20);
	printf("\n");
#endif
	fs_hmac_data(
		buffer,
		fp->node->uid,
		fp->node->name,
		fp->idx,
		fp->node->dummy,
		fp->cluster_idx,
		hmac
	);
	//hexdump(hmac, 0x14);
	aes_reset();
	aes_set_iv(iv);
	aes_set_key(otp.nand_key);
	aes_encrypt(buffer, buffer, 0x400, 0);

	nand_write_cluster(pageno, buffer, hmac);

}

s32 nandfs_initialize(void)
{
	u32 i;
	u32 supercluster_version = 0;

	if(initialized == 1)
		return 0;

	otp_init();

	nand_reset();

	for(i = 0x7F00; i < 0x7fff; i++) {
		nand_read(i*8, sffs.buffer, NULL);
		if(memcmp(sffs.sffs.magic, "SFFS", 4) != 0)
			continue;
#if NANDFS_VERBOSE >= 2
		printf("Got supercluster with version %d\n", sffs.sffs.version);
#endif
		if(supercluster == 0 ||
		   sffs.sffs.version > supercluster_version) {
			supercluster = i*8;
			supercluster_version = sffs.sffs.version;
		}
	}

	if(supercluster == 0) {
		printf("no supercluster found. "
				 " your nand filesystem is seriously broken...\n");
		return -1;
	}

	for(i = 0; i < 16; i++) {
#if NANDFS_VERBOSE >= 2
		printf("reading...\n");
#endif
		nand_read_cluster(supercluster + i*8,
				(sffs.buffer) + (i * PAGE_SIZE * 8));
	}

	initialized = 1;

	fs_hmac_set_key(otp.nand_hmac, 20);
	return 0;
}

void nandfs_writemeta(void)
{
	u32 i;
	static u8 hmac[20] __attribute__((aligned(128)));

	// This does not make a copy like it's supposed to
	// but who cares

	fs_hmac_meta(sffs.buffer, supercluster/8, hmac);
	
	for(i = 0; i < 16; i++) {
#if NANDFS_VERBOSE >= 2
		printf("HMAC:\n");
		hexdump(hmac, 20);
#endif
		u8 *buffer = sffs.buffer + i * PAGE_SIZE * 8;
		nand_write_cluster(supercluster + i*8, buffer, i == 15 ? hmac : NULL);
	}

	nand_lazy_write((u32) -1, NULL, NULL); // Commit
}

u32 nandfs_get_usage(void) {
	u32 i;
	int used_clusters = 0;
	for (i=0; i < sizeof(sffs.sffs.cluster_table) / sizeof(u16); i++) {
		printf("Cluster %x is %04x\n", i, sffs.sffs.cluster_table[i]);
		if(sffs.sffs.cluster_table[i] != NANDFS_CLUSTER_FREE) used_clusters++;
	}
	printf("Used clusters: %d\n", used_clusters);
	return 1000 * used_clusters / (sizeof(sffs.sffs.cluster_table)/sizeof(u16));
}

static void nandfs_walker(struct _nandfs_file_node *cur, int tabs)
{
	char fn[13];
	int i;
	fn[12] = 0;
	while(1) {
		memcpy(fn, cur->name, 12);
		for(i = 0; i < tabs; i++) printf("	  ");
		printf("%s [%d %d]\n", fn, cur->attr & 3, cur->attr);
		if((cur->attr&3) == NANDFS_ATTR_DIR
			 && (s16)(cur->first_child&0xffff) != (s16)0xffff) {
			nandfs_walker(&sffs.sffs.files[cur->first_child], tabs+1);
		}
		if((cur->sibling & 0xffff) == 0xffff) return;
		cur = &sffs.sffs.files[cur->sibling];
	}
}

s32 nandfs_walk()
{
	if(initialized != 1) 
		return -1;
	nandfs_walker(sffs.sffs.files, 0);
	return 0;
}

static struct _nandfs_file_node *nandfs_new_node(u32 uid, u16 gid, u8 attr, u8 user_perm, u8 group_perm, u8 other_perm)
{
	u32 i;
	attr = attr | ((user_perm&3) << 6) | ((group_perm&3) << 4) | ((other_perm&3) << 2);
	struct _nandfs_file_node *node;
	struct _nandfs_file_node empty;
	memset(&empty, 0, sizeof(struct _nandfs_file_node));
	
	for(i = 0; i < 6143; i++) {
		node = &sffs.sffs.files[i];
		if(memcmp(node, &empty, sizeof(struct _nandfs_file_node)) == 0) {
			// We can use it
			break;
		}
	}
	if(i == 6143) {
		return NULL;
	}

	node->attr = attr;
	node->wtf = 0;
	if((attr & 3) == 2) {
		node->first_child = 0xffff;
	} else {
		node->first_cluster = NANDFS_CLUSTER_LAST;
	}
	node->sibling = 0xffff;
	node->size = 0;
 	node->uid = uid;
	node->gid = gid;
	node->dummy = 0;

    return node;
}

s32 nandfs_format()
{
	u32 i;
	for (i=0; i < sizeof(sffs.sffs.cluster_table) / sizeof(u16); i++) {
#if NANDFS_VERBOSE >= 2
		printf("Cluster %x was %04x\n", i, sffs.sffs.cluster_table[i]);
#endif
		if(sffs.sffs.cluster_table[i] < 0xfff0)
			sffs.sffs.cluster_table[i] = NANDFS_CLUSTER_FREE;
	}
	memset(&sffs.sffs.files[1], 0, sizeof(sffs.sffs.files) - sizeof(struct _nandfs_file_node)); // Preserve '/'
	sffs.sffs.files[0].first_child = 0xffff;
	return 0;
}

s32 nandfs_create(const char *path, u32 uid, u16 gid, u8 attr, u8 user_perm, u8 group_perm, u8 other_perm)
{
	char *ptr, *ptr2;
	u32 len;
	struct _nandfs_file_node *cur = sffs.sffs.files;

#if NANDFS_VERBOSE >= 1
	printf("nandfs_create: %s\n", path);
#endif
	if (initialized != 1)
		return -1;

	if(strcmp(cur->name, "/") != 0) {
		printf("your nandfs is corrupted. fixit!\n");
		return -1;
	}

	//cur = &sffs.sffs.files[cur->first_child];

	ptr = (char *)path;
	ptr2 = ptr + 1;
	len = 1;
	do {

		for (;;) {
#if NANDFS_VERBOSE >= 1
			printf("walking %s [%d] fc=%x\n", cur->name, cur->attr & 3, cur->first_cluster);
#endif
			if(ptr2 != NULL && strncmp(cur->name, ptr, len) == 0
				 && strnlen(cur->name, 12) == len
				 && (cur->attr&3) == NANDFS_ATTR_DIR) {
				if((cur->first_child&0xffff) != 0xffff) {
					// Open dir
					cur = &sffs.sffs.files[cur->first_child];
					ptr = ptr2-1;
					break;
				} else {
					struct _nandfs_file_node *new = nandfs_new_node(uid, gid, attr, user_perm, group_perm, other_perm);
					if(!new) return -1;
					cur->first_child = new - sffs.sffs.files;
#if NANDFS_VERBOSE >= 1
					printf("I will create NEW FIRST CHILD ptr=%s ptr2=%s\n", ptr, ptr2);
#endif
					memcpy(new->name, ptr2, strlen(ptr2));
					return 0;
				}
			} else if(ptr2 == NULL &&
				   strncmp(cur->name, ptr, len) == 0 &&
				   strnlen(cur->name, 12) == len) {
				// I guess it already exists?
				return -1;
			} else if((cur->sibling&0xffff) != 0xffff) {
				// Next file in dir
				cur = &sffs.sffs.files[cur->sibling];
			} else {
				// It wasn't found.
				// So we should create it.
				struct _nandfs_file_node *new = nandfs_new_node(uid, gid, attr, user_perm, group_perm, other_perm);
				if(!new) return -1;
				cur->sibling = new - sffs.sffs.files;
#if NANDFS_VERBOSE >= 1
				printf("I will create NEW SIBLING ptr=%s ptr2=%s\n", ptr, ptr2);
#endif
				memcpy(new->name, ptr, strlen(ptr));
				return 0;
			}
		}
		if(ptr2 == NULL) break;
		ptr++;	
		ptr2 = strchr(ptr, '/');
		if (ptr2 == NULL)
			len = strlen(ptr);
		else {
			ptr2++;
			len = ptr2 - ptr - 1;
		}
		if (len > 12)
		{
			printf("invalid length: %s %s %s [%d]\n",
					ptr, ptr2, path, len);
			return -1;
		}
	} while(1);

	return -2; // WTF?
}

s32 nandfs_open(struct nandfs_fp *fp, const char *path)
{
	char *ptr, *ptr2;
	u32 len;
	struct _nandfs_file_node *last, *cur = sffs.sffs.files;
	u8 last_type;

#if NANDFS_VERBOSE >= 1
	printf("nandfs_open: open %s\n", path);
#endif

	if (initialized != 1)
		return -1;

	memset(fp, 0, sizeof(*fp));

	if(strcmp(cur->name, "/") != 0) {
		printf("your nandfs is corrupted. fixit!\n");
		return -1;
	}

	last = cur;
	last_type = NANDFS_LAST_CHILD;
	cur = &sffs.sffs.files[cur->first_child];

	ptr = (char *)path;
	do {
		ptr++;
		ptr2 = strchr(ptr, '/');
		if (ptr2 == NULL)
			len = strlen(ptr);
		else {
			ptr2++;
			len = ptr2 - ptr - 1;
		}
		if (len > 12)
		{
			printf("invalid length: %s %s %s [%d]\n",
					ptr, ptr2, path, len);
			return -1;
		}

		for (;;) {
#if NANDFS_VERBOSE >= 1
			printf("walking %s [%d]\n", cur->name, cur->attr & 3);
#endif
			if(ptr2 != NULL && strncmp(cur->name, ptr, len) == 0
				 && strnlen(cur->name, 12) == len
				 && (cur->attr&3) == NANDFS_ATTR_DIR) {
				if((cur->first_child&0xffff) != 0xffff) {
					// Open dir
					last = cur;
					last_type = NANDFS_LAST_CHILD;
					cur = &sffs.sffs.files[cur->first_child];
					ptr = ptr2-1;
					break;
				} else {
					return -1;
				}
			} else if(ptr2 == NULL &&
				   strncmp(cur->name, ptr, len) == 0 &&
				   strnlen(cur->name, 12) == len /*&&
				   (cur->attr&3) == 1*/) {
				break;
			} else if((cur->sibling&0xffff) != 0xffff) {
				// Next file in dir
				last = cur;
				last_type = NANDFS_LAST_SIBLING;
				cur = &sffs.sffs.files[cur->sibling];
			} else {
				// It wasn't found.
				return -1;
			}
		}
		
	} while(ptr2 != NULL);

	fp->first_cluster = cur->first_cluster;
	fp->cur_cluster = fp->first_cluster;
	fp->cluster_idx = 0;
	fp->offset = 0;
	fp->size = cur->size;
	fp->node = cur;
	fp->idx = cur - sffs.sffs.files;
	fp->last = last;
	fp->last_type = last_type;
	return 0;
}

s32 nandfs_read(u8 *ptr, u32 size, u32 nmemb, struct nandfs_fp *fp)
{
	u32 total = size*nmemb;
	u32 copy_offset, copy_len;

	if (initialized != 1)
		return -1;

	if (fp->offset + total > fp->size)
		total = fp->size - fp->offset;

	if (total == 0)
		return 0;

	while(total > 0) {
        if(fp->cur_cluster > 0xfff0) {
            return size*nmemb - total;
        }
		if(fp->offset / (PAGE_SIZE * 8) == fp->cluster_idx) {
            copy_offset = fp->offset % (PAGE_SIZE * 8);
            copy_len = (PAGE_SIZE * 8) - copy_offset;
            if(copy_len > total)
                copy_len = total;
            nand_read_decrypted_cluster(fp->cur_cluster*8, buffer);

            memcpy(ptr, buffer + copy_offset, copy_len);
            total -= copy_len;
            fp->offset += copy_len;
            ptr += copy_len;

            if ((copy_offset + copy_len) >= (PAGE_SIZE * 8)) {
                fp->cur_cluster = sffs.sffs.cluster_table[fp->cur_cluster];
                fp->cluster_idx++;
            }
        } else {
            fp->cur_cluster = sffs.sffs.cluster_table[fp->cur_cluster];
            fp->cluster_idx++;
        }
	}

	return size*nmemb;
}

s32 nandfs_write(const u8 *ptr, u32 size, u32 nmemb, struct nandfs_fp *fp)
{
	u32 total = size*nmemb;
	u32 copy_offset, copy_len;
	s32 next_cluster;
	u32 i;

	if (initialized != 1)
		return -1;

	/*if (fp->offset + total > fp->size)
		total = fp->size - fp->offset;*/
#if NANDFS_VERBOSE >= 2
    printf("total=%d fp->cur_cluster=%x\n", total, fp->cur_cluster);
#endif

	if (total == 0)
		return 0;

	while(total > 0) {
		if(fp->cur_cluster > 0xfff0) {
			// And actually reserve a new cluster
#if NANDFS_VERBOSE >= 2
			printf("fp->cur_cluster = %x idx=%x reserving new\n", fp->cur_cluster, fp->cluster_idx);
#endif

			for(i = 0; i < sizeof(sffs.sffs.cluster_table) / sizeof(u16); i++) {
				if(sffs.sffs.cluster_table[i] == NANDFS_CLUSTER_FREE) {
					static u8 spare[64] __attribute__((aligned(128)));
					u8 good = 0;
					nand_read(i*8, NULL, spare);
					if(spare[0] == 0xff) {
						// Good page
						nand_read(i*8+1, NULL, spare);
						if(spare[0] == 0xff) {
							// It's a good block I guess
							good = 1;
						}
					}
#if NANDFS_VERBOSE >= 2
					printf("Cluster %d is %s\n", i, good ? "good" : "bad");
#endif
					if(good) {
                        if(fp->first_cluster > 0xfff0) {
                            fp->first_cluster = fp->node->first_cluster = i;
                        } else {
                            fp->cur_cluster = fp->first_cluster;
                            while(1) {
                                next_cluster = sffs.sffs.cluster_table[fp->cur_cluster];
                                if(next_cluster > 0xfff0) break;
                                fp->cur_cluster = next_cluster;
                            }
                            sffs.sffs.cluster_table[fp->cur_cluster] = i;
                        }
						sffs.sffs.cluster_table[i] = NANDFS_CLUSTER_LAST;
						fp->cur_cluster = i;
						break;
					}
				}
			}
			if(i == sizeof(sffs.sffs.cluster_table) / sizeof(u16)) {
				// We're out of space.  Crap.
				printf("Warning: Out of space\n");
				return size*nmemb - total;
			}
		}

		if(fp->offset / (PAGE_SIZE * 8) == fp->cluster_idx) {
			copy_offset = fp->offset % (PAGE_SIZE * 8);
			copy_len = (PAGE_SIZE * 8) - copy_offset;
			if(copy_len > total)
				copy_len = total;

			nand_read_decrypted_cluster(fp->cur_cluster*8, buffer);
#if NANDFS_VERBOSE >= 2
			printf("Total is %d copy_len is %d\n", total, copy_len);
			printf("-- copy_offset = %d\n", copy_offset);
			hexdump(buffer + copy_offset, 0x20);
			printf("--\n");
			hexdump(ptr, 0x20);
			printf("--\n");
#endif
			memcpy(buffer + copy_offset, ptr, copy_len);
			nand_write_decrypted_cluster(fp->cur_cluster*8, buffer, fp);

			total -= copy_len;
			fp->offset += copy_len;
			ptr += copy_len;

			if ((copy_offset + copy_len) >= (PAGE_SIZE * 8)) {
				fp->cur_cluster = sffs.sffs.cluster_table[fp->cur_cluster];
				fp->cluster_idx++;
			}
		} else {
            fp->cur_cluster = sffs.sffs.cluster_table[fp->cur_cluster];
            fp->cluster_idx++;
        }
	}

	// Do we need to increase size?
	if(fp->offset > fp->size) {
		fp->size = fp->node->size = fp->offset;	
	}

	nand_lazy_write((u32) -1, NULL, NULL); // Commit

	return size*nmemb;
}

s32 nandfs_seek(struct nandfs_fp *fp, s32 offset, u32 whence)
{
	if (initialized != 1)
		return -1;

	switch (whence) {
	case NANDFS_SEEK_SET:
		if (offset < 0)
			return -1;
		if ((u32)offset > fp->size)
			return -1;

		fp->offset = offset;
		break;

	case NANDFS_SEEK_CUR:
		if ((fp->offset + offset) > fp->size ||
			(s32)(fp->offset + offset) < 0)
			return -1;
		fp->offset += offset;
		break;

	case NANDFS_SEEK_END:
	default:
		if ((fp->size + offset) > fp->size ||
			(s32)(fp->size + offset) < 0)
			return -1;
		fp->offset = fp->size + offset;
		break;
	}

	int skip = fp->offset;
	fp->cur_cluster = fp->first_cluster;
	fp->cluster_idx = 0;
	while (skip > (2048*8)) {
		fp->cur_cluster = sffs.sffs.cluster_table[fp->cur_cluster];
		fp->cluster_idx++;
		skip -= 2048*8;
	}

	return 0;
}

s32 nandfs_delete(struct nandfs_fp *fp)
{
#if NANDFS_VERBOSE >= 1
	printf("Deleting %s size %d\n", fp->node->name, fp->node->size);
#endif
	// Get rid of the clusters	
    if((fp->node->attr & 3) == 1) {
        fp->cur_cluster = fp->first_cluster;
        while(fp->cur_cluster <= 0xfff0) {
            printf("Deleting cluster %x\n", fp->cur_cluster);
            s16 clus = fp->cur_cluster;
            fp->cur_cluster = sffs.sffs.cluster_table[clus];
            sffs.sffs.cluster_table[clus] = NANDFS_CLUSTER_FREE;
        }
    }
	// Now get rid of the file
#if NANDFS_VERBOSE >= 1
	printf("last: %x [%s] last_type: %d sibling: %d\n", fp->last - sffs.sffs.files, fp->last->name, fp->last_type, fp->node->sibling);
#endif
	switch(fp->last_type) {
	case NANDFS_LAST_SIBLING:
		fp->last->sibling = fp->node->sibling;
		break;
	case NANDFS_LAST_CHILD:
		fp->last->first_child = fp->node->sibling;
		break;
	}
	memset(fp->node, 0, sizeof(struct _nandfs_file_node));
	return 0;
}

void nandfs_test()
{
	// Remove this
	int i;
	struct _nandfs_file_node empty;
	memset(&empty, 0, sizeof(struct _nandfs_file_node));
	for(i = 0; i < 6143; i++) {
		struct _nandfs_file_node *file = &sffs.sffs.files[i];
		if(memcmp(&empty, file, sizeof(struct _nandfs_file_node)) == 0) continue;
		char name[13];
		name[12] = 0;
		memcpy(name, file, 12);
		printf("%s fc:%x\n", name, file->first_cluster);
		hexdump(file, sizeof(*file)); 
	}
	for(i = 0; i < 32768; i++) {
		u16 clus = sffs.sffs.cluster_table[i];
		printf("Cluster %x is %x\n", i, clus);
	}
}

