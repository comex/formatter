/*
	BootMii - a Free Software replacement for the Nintendo/BroadOn IOS.
	Requires mini.

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#ifndef __MAIN_H__
#define __MAIN_H__

void hexdump(void *d, int len);

#ifdef MSPACES
#include "malloc.h"
extern mspace mem2space;
#endif

#endif
