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
#include "otp.h"
#include "nandfs.h"
#include "string.h"
#include "sha1.h"
#include "wad.h"
#include "es.h"
#include "aes.h"
#include "malloc.h"
#include "main.h"

#define ASSERT(x) do { if(!(x)) { printf("Assert failure: %s in %s:%d\n", #x, __FILE__, __LINE__); return -1; } } while(0)

#define ALIGN(x, y) ((x) % (y) == 0 ? x : (((x) + (y)) - ((x) % (y))))

s32 wad_install(FIL *fil)
{
	u16 i;
	u32 br;
	struct wadheader hdr __attribute__((aligned(32)));
	f_lseek(fil, 0);
	ASSERT(!f_read(fil, &hdr, sizeof(hdr), &br));
	ASSERT(br == sizeof(hdr));

	u32 offset = 0;

	static u8 key[16] __attribute__((aligned(64))) = {0,};
	static u8 iv[16] __attribute__((aligned(64))) = {0,};

	ASSERT(hdr.hdr_size == 0x20);

	offset += ALIGN(hdr.hdr_size, 0x40);
	offset += ALIGN(hdr.certs_size, 0x40);
	struct tik *tik = memalign(32, hdr.tik_size);
	ASSERT(tik);
	f_lseek(fil, offset);
	ASSERT(!f_read(fil, tik, hdr.tik_size, &br));
	ASSERT(br == hdr.tik_size);
	offset += ALIGN(hdr.tik_size, 0x40);
	struct tmd *tmd = memalign(32, hdr.tmd_size);
	ASSERT(tmd);
	f_lseek(fil, offset);
	ASSERT(!f_read(fil, tmd, hdr.tmd_size, &br));
	ASSERT(br == hdr.tmd_size);
	offset += ALIGN(hdr.tmd_size, 0x40);

	hexdump(tik, hdr.tik_size);
	hexdump(tmd, hdr.tmd_size);

	otp_init();

	// Get the title key
	aes_reset();
	aes_set_key(otp.common_key);
	memcpy(iv, &tik->title_id, 8);
	aes_set_iv(iv);
	printf("common:\n");
	hexdump(otp.common_key, 16);
	printf("iv:\n");
	hexdump(iv, 16);
	printf("ctk:\n");
	hexdump(tik->cipher_title_key, 16);
	memcpy(iv, tik->cipher_title_key, 16);
	aes_decrypt(iv, key, 1, 0);
	memset(iv, 0, 16);

	printf("title key:\n");
	hexdump(key, 16);

	printf("es_addtitle: %d\n", es_addtitle(tmd, tik));
	printf("num contents: %d\n", tmd->num_contents);
	printf("%08x %08x\n", tmd, tik);
	for(i = 0; i < tmd->num_contents; i++) {
#ifdef MSPACES
		u8 *content = mspace_memalign(mem2space, 32, ALIGN(tmd->contents[i].size, 0x40));
#else
		u8 *content = (void *)0x91000000;//memalign(32, ALIGN(tmd->contents[i].size, 0x40));
#endif
		ASSERT(content);

		f_lseek(fil, offset);
		printf("read content --> %08x size %08x", content, (u32) tmd->contents[i].size);
		ASSERT(!f_read(fil, content, ALIGN(tmd->contents[i].size, 0x40), &br));
		printf("done\n");
		ASSERT(br == ALIGN(tmd->contents[i].size, 0x40));

		memcpy(iv, &tmd->contents[i].index, 2);
		aes_reset();
		aes_set_key(key);
		aes_set_iv(iv);
		printf("an IV:\n");
		hexdump(iv, 16);
		aes_decrypt(content, content, ALIGN(tmd->contents[i].size, 0x40) / 16, 0);
		printf("decrypted ");
		ASSERT(!es_addtitlecontent(tmd, tmd->contents[i].index, content, tmd->contents[i].size));

		offset += ALIGN(tmd->contents[i].size, 0x40);
#ifdef MSPACES
		mspace_free(mem2space, content);
#else
		//free(content);
#endif
	}

	free(tmd);
	free(tik);

	return 0;
}
