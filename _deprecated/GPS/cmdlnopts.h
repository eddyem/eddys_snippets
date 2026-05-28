/*
 * cmdlnopts.h - comand line options for parceargs
 *
 * Copyright 2013 Edward V. Emelianoff <eddy@sao.ru>
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
#ifndef __CMDLNOPTS_H__
#define __CMDLNOPTS_H__

#include "parceargs.h"

// GSV, RMC, GSA, GGA, GLL, VTG, TXT
extern char *GPmsgs[];
typedef enum{
	GPGSV = 0,
	GPRMC,
	GPGSA,
	GPGGA,
	GPGLL,
	GPVTG,
	GPMAXMSG
}GPmsg_type;

/*
 * here are some typedef's for global data
 */
typedef struct{
	char *devpath;               // device path
	int pollubx;                 // flag of polling ubx00
	double pollinterval;         // ubx00 polling interval (in seconds)
	int block_msg[GPMAXMSG];     // array of messages: 1-show, 0-block
	double polltmout;            // polling timeout (program ends after this interval)
	int stationary;              // configure as stationary
	int gettimediff;             // calculate mean time difference
	int meancoords;              // calculate mean coordinates
	int silent;                  // don't write intermediate messages
	int date;                    // print date for initial time setup
}glob_pars;

glob_pars *parce_args(int argc, char **argv);

#endif // __CMDLNOPTS_H__
