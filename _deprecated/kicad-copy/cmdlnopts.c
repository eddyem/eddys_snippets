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
#include "copy_pcb.h"
#include "cmdlnopts.h"

/*
 * here are global parameters initialisation
 */
glob_pars G;
int help = 0;

/*
 * Define command line options by filling structure:
 *	name	has_arg	flag	val		type		argptr			help
*/
myoption cmdlnopts[] = {
	{"help",	0,	NULL,	'h',	arg_int,	APTR(&help),		N_("show this help")},
	{"format",	1,	NULL,	'f',	arg_string,	APTR(&G.format),	N_("printf-like format of similar items (%s - MUST BE - old name; %n - new num)")},
	{"nparts",	1,	NULL,	'N',	arg_int,	APTR(&G.nparts),	N_("Number of copies")},
	{"list",	1,	NULL,	'L',	arg_string,	APTR(&G.list),		N_("comma-separated list of sheets")},
	{"input",	1,	NULL,	'i',	arg_string,	APTR(&G.ifile),		N_("input file name")},
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
	memset(&G, 0, sizeof(glob_pars)); // clear all
	// format of help: "Usage: progname [args]\n"
	change_helpstring("Usage: %s [args]\n\n\tWhere args are:\n");
	// parse arguments
	parceargs(&argc, &argv, cmdlnopts);
	if(help) showhelp(-1, cmdlnopts);
	if(argc > 0){
		printf("\nIgnore argument[s]:\n");
		for (i = 0; i < argc; i++)
			printf("\t%s\n", argv[i]);
	}
	return &G;
}

