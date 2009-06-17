/*
	BootMii - a Free Software replacement for the Nintendo/BroadOn IOS.
	Requires mini.

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#ifndef __WAD_H__
#define __WAD_H__

struct wadheader {
	u32 hdr_size;
	u32 type;
	u32 certs_size;
	u32 reserved;
	u32 tik_size;
	u32 tmd_size;
	u32 data_size;
	u32 footer_size;
};

// Warning: This is destructive and will mess up (actually decrypt) your wa
s32 wad_install(void *wad);
#endif
