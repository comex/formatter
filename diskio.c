/*
	diskio.c -- glue interface to ElmChan FAT FS driver. Part of the
	BootMii project.

Copyright (C) 2008, 2009	Haxx Enterprises <bushing@gmail.com>
Copyright (C) 2008, 2009	Sven Peter <svenpeter@gmail.com>

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include "bootmii_ppc.h"
#include "ipc.h"
#include "mini_ipc.h"
#include "diskio.h"
#include "string.h"

static u8 *buffer[512] __attribute__((aligned(32)));
#define MAX_SECTORS_PER_READ 512
#define MAX_SECTORS_PER_WRITE 512

DSTATUS disk_initialize (BYTE drv)
{
	(void) drv;

	int state = sd_get_state();

	switch (state) {
	case SDMMC_NO_CARD:
		return STA_NODISK;

	case SDMMC_NEW_CARD:
		if (sd_mount())
			return STA_NOINIT;
		else
			return 0;

	default:
		return 0;
	}
}

DSTATUS disk_status (BYTE drv)
{
	(void) drv;

	int state = sd_get_state();

	switch (state) {
	case SDMMC_NO_CARD:
		return STA_NODISK;

	case SDMMC_NEW_CARD:
		return STA_NOINIT;

	default:
		return 0;
	}
}

DRESULT disk_read (BYTE drv, BYTE *buff, DWORD sector, u32 count)
{
	u32 i;
	DRESULT res;
	(void) drv;

//	printf("%s(%d,%p,%u,%u)\n", __FUNCTION__, drv, buff, sector, count);
	if (count > 1 && ((u32) buff % 64) == 0) {
		while (count != 0) {
			u32 sectors = (count > MAX_SECTORS_PER_READ)? MAX_SECTORS_PER_READ : count;
			res = sd_read(sector, sectors, buff);
			if (res != 0) {
				printf("error %d reading sector %d\n", res, sector);
				return RES_ERROR;
			}
			buff += sectors * 512;
			count -= sectors;
			sector += sectors;
		}
		return RES_OK;
	}

	res = RES_OK;
	for (i = 0; i < count; i++) {
		if (sd_read(sector + i, 1, buffer) != 0) {
			res = RES_ERROR;
			break;
		}

		memcpy(buff + i * 512, buffer, 512);
	}

	return res;
}

#if _READONLY == 0
DRESULT disk_write (BYTE drv, const BYTE *buff,	DWORD sector, u32 count)
{
	u32 i;
	DRESULT res;
	(void) drv;
//	printf("%s(%d,%p,%u,%u)\n", __FUNCTION__, drv, buff, sector, count);

	res = RES_OK;
	if (count > 1 && ((u32) buff % 64) == 0) {
		while (count != 0) {
			u32 sectors = (count > MAX_SECTORS_PER_WRITE)? MAX_SECTORS_PER_WRITE : count;
			res = sd_write(sector, sectors, buff);
			if (res != 0) {
				printf("error %d writing sector %d\n", res, sector);
				return RES_ERROR;
			}
			buff += sectors * 512;
			count -= sectors;
			sector += sectors;
		}
		return RES_OK;
	}

	for (i = 0; i < count; i++) {
		memcpy(buffer, buff + i * 512, 512);
		if (sd_write(sector + i, 1, buffer) != 0) {
			res = RES_ERROR;
			break;
		}
	}

	return res;
}
#endif /* _READONLY */

DRESULT disk_ioctl (BYTE drv, BYTE ctrl, void *buff)
{
	(void) drv;
	u32 *buff_u32 = (u32 *) buff;
	DRESULT res = RES_OK;

	switch (ctrl) {
	case CTRL_SYNC:
		break;
	case GET_SECTOR_COUNT:
		*buff_u32 = sd_getsize();
		break;
	case GET_SECTOR_SIZE:
		*buff_u32 = 512;
		break;
	case GET_BLOCK_SIZE:
		*buff_u32 = 512;
		break;
	default:
		res = RES_PARERR;
		break;
	}

	return res;
}

DWORD get_fattime(void)
{
	return 0; // TODO
}
