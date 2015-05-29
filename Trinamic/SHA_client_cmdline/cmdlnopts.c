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
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "cmdlnopts.h"

/*
 * here are global parameters initialisation
 */
glob_pars G;  // internal global parameters structure
int help = 0; // whether to show help string

glob_pars Gdefault = {
	.comdev    = "/dev/ttyUSB0",
	.relaycmd  = -1,
	.erasecmd  = 0,
	.gotopos   = NULL
};

/*
 * Define command line options by filling structure:
 *	name	has_arg	flag	val		type		argptr			help
*/
myoption cmdlnopts[] = {
	/// "отобразить это сообщение"
	{"help",	0,	NULL,	'h',	arg_int,	APTR(&help),		"show this help"},
	/// "путь к устройству"
	{"comdev",1,	NULL,	'd',	arg_string,	APTR(&G.comdev),	"input device path"},
	/// "включить (1)/выключить (0) реле"
	{"relay", 1,	NULL,	'r',	arg_int,	APTR(&G.relaycmd),	"turn relay on (1)/off (0)"},
	/// "только очистить память контроллера"
	{"erase-old",0,NULL,	'e',	arg_none,	APTR(&G.erasecmd),	"only erase controller's memory"},
	/// "перейти на позицию (refmir/diagmir/shack)"
	{"goto", 1,		NULL,	'g',	arg_string,	APTR(&G.gotopos),	"go to position (refmir/diagmir/shack)"},
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
	void *ptr;
	ptr = memcpy(&G, &Gdefault, sizeof(G)); assert(ptr);
	// format of help: "Usage: progname [args]\n"
	/// "Использование: %s [аргументы]\n\n\tГде аргументы:\n"
	change_helpstring("Usage: %s [args]\n\n\tWhere args are:\n");
	// parse arguments
	parceargs(&argc, &argv, cmdlnopts);
	if(help) showhelp(-1, cmdlnopts);
	if(argc > 0){
		/// "Игнорирую аргумент[ы]:"
		printf("\n%s\n", "Ignore argument[s]:");
		for (i = 0; i < argc; i++)
			printf("\t%s\n", argv[i]);
	}
	return &G;
}

