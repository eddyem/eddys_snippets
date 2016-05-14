/*
 * hsfw.h - functions for work with wheels
 *
 * Copyright 2016 Edward V. Emelianov <eddy@sao.ru, edward.emelianoff@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 */
#pragma once
#ifndef __HSFW_H__
#define __HSFW_H__

#define REG_CLERR		(0x02)
#define REG_CLERR_LEN	(2)
#define REG_STATUS		(0x0a)
#define REG_STATUS_LEN	(6)
#define REG_INFO		(0x0b)
#define REG_INFO_LEN	(7)
#define REG_GOTO		(0x14)
#define REG_GOTO_LEN	(3)
#define REG_HOME		(0x15)
#define REG_HOME_LEN	(3)
#define REG_NAME		(0x16)
#define REG_NAME_LEN	(14)

// absolute max position (for any wheel)
#define ABS_MAX_POS		(8)

enum name_cmd{
	RESTORE_DEFVALS = 1,
	RENAME_FILTER,
	FILTER_NAME,
	RENAME_WHEEL,
	WHEEL_NAME
};

void check_args();
void process_args();
#endif // __HSFW_H__
