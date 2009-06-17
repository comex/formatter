/*
	BootMii - a Free Software replacement for the Nintendo/BroadOn IOS.
	Requires mini.

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include "otp.h"

otp_t otp __attribute__((aligned(32)));
static u8 otp_initialized = 0;

void otp_init()
{
	if(otp_initialized) return;
	otp_initialized = 1;
	getotp(&otp);
}
