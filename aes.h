/*
	BootMii - a Free Software replacement for the Nintendo/BroadOn IOS.
	Requires mini.

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#ifndef __AES_H__
#define __AES_H__
#define HARDWARE_AES 0
void my_aes_set_key(u8 *key);
void my_aes_decrypt(u8 *iv, u8 *inbuf, u8 *outbuf, unsigned long long len);
void my_aes_encrypt(u8 *iv, u8 *inbuf, u8 *outbuf, unsigned long long len);

void aes_reset(void);
void aes_set_key(u8 *key);
void aes_set_iv(u8 *iv);
void aes_decrypt(u8 *src, u8 *dst, u32 blocks, u8 keep_iv);
void aes_encrypt(u8 *src, u8 *dst, u32 blocks, u8 keep_iv);

#endif

