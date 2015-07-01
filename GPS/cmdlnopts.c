/*
 * cmdlnopts.c - the only function that parce cmdln args and returns glob parameters
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
#include "cmdlnopts.h"
#include "usefull_macros.h"

/*
 * here are global parameters initialisation
 */
glob_pars G;  // internal global parameters structure
int help = 0; // whether to show help string

glob_pars Gdefault = {
	 .devpath        = "/dev/ttyUSB0"
	,.pollubx        = 0
	,.pollinterval   = 1.
	,.block_msg      = {0,1,0,0,0,0}   // all exept RMC are blocked by default
	,.polltmout      = 10.
	,.stationary     = 0
	,.gettimediff    = 0
	,.meancoords     = 0
};

/*
 * Define command line options by filling structure:
 *	name	has_arg	flag	val		type		argptr			help
*/
myoption cmdlnopts[] = {
	/// "отобразить это сообщение"
	{"help",	0,	NULL,	'h',	arg_int,	APTR(&help),		N_("show this help")},
	/// "путь к устройству"
	{"device",1,	NULL,	'd',	arg_string,	APTR(&G.devpath),	N_("device path")},
	{"poll-udx", 0,	NULL,	'p',	arg_none,	APTR(&G.pollubx),	N_("poll UDX,00")},
	{"pollinterval",1,NULL,	'i',	arg_double,	APTR(&G.pollinterval),N_("polling interval")},
	{"no-rmc",0,&G.block_msg[GPRMC],	0,	arg_none,	NULL,	N_("block RMC message")},
	{"gsv",	0,	&G.block_msg[GPGSV],	1,	arg_none,	NULL,	N_("process GSV message")},
	{"gsa",	0,	&G.block_msg[GPGSA],	1,	arg_none,	NULL,	N_("process GSA message")},
	{"gga",	0,	&G.block_msg[GPGGA],	1,	arg_none,	NULL,	N_("process GGA message")},
	{"gll",	0,	&G.block_msg[GPGLL],	1,	arg_none,	NULL,	N_("process GLL message")},
	{"vtg",	0,	&G.block_msg[GPVTG],	1,	arg_none,	NULL,	N_("process VTG message")},
	{"timeout", 1,	NULL,	't',	arg_double,	APTR(&G.polltmout), N_("polling timeout")},
	{"stationary",0, NULL,	's',	arg_int,	APTR(&G.stationary),N_("configure as stationary")},
	{"timediff",0,	NULL,	'T',	arg_int,	APTR(&G.gettimediff),N_("calculate mean time difference")},
	{"coords",0,	NULL,	'C',	arg_int,	APTR(&G.meancoords), N_("calculate mean coordinates")},
	// ...
	end_option
};

/**
 * Parce command line options and return dynamically allocated structure
 * 		to global parameters
 * @param argc - copy of argc from main
 * @param argv - copy of argv from main
 * @return allocated structure with global parameters
 */
glob_pars *parce_args(int argc, char **argv){
	int i;
	memcpy(&G, &Gdefault, sizeof(G));
	/// "Использование: %s [аргументы]\n\n\tГде аргументы:\n"
	change_helpstring(_("Usage: %s [args]\n\n\tWhere args are:\n"));
	// parse arguments
	parceargs(&argc, &argv, cmdlnopts);
	if(help) showhelp(-1, cmdlnopts);
	if(argc > 0){
		/// "Игнорирую аргумент[ы]:"
		printf("\n%s\n", _("Ignore argument[s]:"));
		for (i = 0; i < argc; i++)
			printf("\t%s\n", argv[i]);
	}
	return &G;
}

