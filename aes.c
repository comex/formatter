/*
	BootMii - a Free Software replacement for the Nintendo/BroadOn IOS.
	Requires mini.

	aes

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/


#include "bootmii_ppc.h"
#include "string.h"
#include "aes.h"
#include "malloc.h"

#if !HARDWARE_AES

static u8 aes_iv[16];

void aes_reset(void) {}
void aes_set_iv(u8 *iv)
{
	memcpy(aes_iv, iv, 16);
}
void aes_set_key(u8 *key)
{
	my_aes_set_key(key);
}
void aes_decrypt(u8 *src, u8 *dst, u32 blocks, u8 keep_iv)
{
	(void)keep_iv;
	if(src == dst) {
		u8 *p = (void *)0x92000000;//malloc(16*blocks);
		my_aes_decrypt(aes_iv, src, p, 16*blocks);
		memcpy(dst, p, 16*blocks);
		//free(p);
	} else
		my_aes_decrypt(aes_iv, src, dst, 16*blocks);
}
void aes_encrypt(u8 *src, u8 *dst, u32 blocks, u8 keep_iv)
{
	(void)keep_iv;
	if(src == dst) {
		u8 *p = (void *)92000000;//malloc(16*blocks);
		my_aes_encrypt(aes_iv, src, p, 16*blocks);
		memcpy(dst, p, 16*blocks);
		//free(p);
	} else
		my_aes_encrypt(aes_iv, src, dst, 16*blocks);
}
#endif
