/*
	BootMii - a Free Software replacement for the Nintendo/BroadOn IOS.
	Requires mini.

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#ifndef __OTP_H__
#define __OTP_H__

#include "bootmii_ppc.h"
#include "ipc.h"
#include "mini_ipc.h"

typedef struct otp_t
{
	u8 boot1_hash[20];
	u8 common_key[16];
	u32 ng_id;
	union { // first two bytes of nand_hmac overlap last two bytes of ng_priv. no clue why
		struct {
			u8 ng_priv[30];
			u8 _wtf1[18];
		};
		struct {
			u8 _wtf2[28];
			u8 nand_hmac[20];
		};
	};
	u8 nand_key[16];
	u8 rng_key[16];
	u32 unk1;
	u32 unk2; // 0x00000007
} __attribute__((packed)) otp_t;

extern otp_t otp;
void otp_init();
#endif
