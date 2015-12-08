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

/*
 * here are global parameters initialisation
 */

/*
 * Define command line options by filling structure:
 *	name	has_arg	flag	val		type		argptr			help
*/
myoption cmdlnopts[] = {
	{"help",	0,	NULL,	'h',	arg_int,	APTR(&help),		N_("show this help")},
	// ...
	end_option
};


/**
 * Parse string of suboptions (--option=name1=var1:name2=var2... or -O name1=var1,name2=var2...)
 * Suboptions could be divided by colon or comma
 *
 * !!NAMES OF SUBOPTIONS ARE CASE-UNSENSITIVE!!!
 *
 * @param arg (i)    - string with description
 * @param V (io) - pointer to suboptions array (result will be stored in sopts->val)
 * @return TRUE if success
 */
bool get_suboptions(void *arg, suboptions *V){
	char *tok, *val, *par;
	int i;
	tok = strtok(arg, ":,");
	do{
		if((val = strchr(tok, '=')) == NULL){ // wrong format
			WARNX(_("Wrong format: no value for keyword"));
			return FALSE;
		}
		*val++ = '\0';
		par = tok;
		for(i = 0; V[i].val; i++){
			if(strcasecmp(par, V[i].par) == 0){ // found parameter
				if(V[i].isdegr){ // DMS
					if(!get_radians(V[i].val, val)) // wrong angle
						return FALSE;
					DBG("Angle: %g rad\n", *(V[i].val));
				}else{ // simple float
					if(!myatof(V[i].val, val)) // wrong number
						return FALSE;
					DBG("Float val: %g\n", *(V[i].val));
				}
				break;
			}
		}
		if(!V[i].val){ // nothing found - wrong format
			WARNX(_("Bad keyword!"));
			return FALSE;
		}
	}while((tok = strtok(NULL, ":,")));
	return TRUE;
}

/**
 * functions of subargs parcing can looks as this
 */
bool get_mir_par(void *arg, int N _U_){
	suboptions V[] = { // array of mirror parameters and string keys for cmdln pars
		{&M.D,		"diam",		FALSE},
		{&M.F,		"foc",		FALSE},
		{&M.Zincl,	"zincl",	TRUE},
		{&M.Aincl,	"aincl",	TRUE},
		{&M.objA,	"aobj",		TRUE},
		{&M.objZ,	"zobj",		TRUE},
		{&M.foc,	"ccd",		FALSE},
		{0,0,0}
	};
	return get_suboptions(arg, V);
}

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
	ptr = memcpy(&M, &Mdefault, sizeof(M)); assert(ptr);
	G.Mirror = &M;
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

