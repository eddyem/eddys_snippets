/*
 * cmdlnopts.c - the only function that parse cmdln args and returns glob parameters
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
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <math.h>
#include "cmdlnopts.h"

#define RAD 57.2957795130823
#define D2R(x) ((x) / RAD)
#define R2D(x) ((x) * RAD)

/*
 * here are global parameters initialisation
 */
int help;
glob_pars  G;
mirPar     M;

int rewrite_ifexists = 0, // rewrite existing files == 0 or 1
	verbose = 0; // each -v increments this value, e.g. -vvv sets it to 3
//            DEFAULTS
// default global parameters
glob_pars const Gdefault = {
	NULL,	// size of initial array of surface deviations
	100,	// size of interpolated S0
	1000,	// resulting image size
	10000,	// amount of photons falled to one pixel of S1 by one iteration
	0,		// add to mask random numbers
	NULL,	// amplitude of added random noice
	NULL,	// mirror
	0,		// rest num
	NULL,	// rest pars
	NULL,	// str
	NULL	// filter
};
//default mirror parameters
mirPar const Mdefault = {
	6.,		// diameter
	24.024,	// focus
	0.,		// inclination from Z axe (radians)
	0.			// azimuth of inclination (radians)
};

// here are functions & definitions for complex parameters (with suboptions etc)
bool get_mir_par(void *arg);
const char MirPar[] = "set mirror parameters, arg=[diam=num:foc=num:Zincl=ang:Aincl=ang]\n" \
		"\t\t\tALL DEGREES ARE IN FORMAT [+-][DDd][MMm][SS.S] like -10m13.4 !\n" \
		"\t\tdiam  - diameter of mirror\n" \
		"\t\tfoc   - mirror focus ratio\n" \
		"\t\tZincl - inclination from Z axe\n" \
		"\t\tAincl - azimuth of inclination";

const char FilPar[] = "set filter parameters, arg=[type=type:xsz=num:ysz=num]\n" \
		"\t\ttype     - filter type(med, lap, lg)\n" \
		"\t\txsz,ysz  - area size (default is 3x3)";

/*
 * Define command line options by filling structure:
 *	name	has_arg	flag	val		type		argptr			help
*/
myoption cmdlnopts[] = {
	// set 1 to param despite of its repeating number:
	{"help",	NO_ARGS,	NULL,	'h',	arg_int,	APTR(&help),		"show this help"},
	// simple integer parameter with obligatory arg:
	{"int-size",NEED_ARG,	NULL,	'i',	arg_int,	APTR(&G.S_interp),	"size of interpolated array of surface deviations"},
	{"image-size",NEED_ARG,NULL,	'I',	arg_int,	APTR(&G.S_image),	"resulting image size"},
	{"N-photons",NEED_ARG,NULL,	'N',	arg_int,	APTR(&G.N_phot), 	"amount of photons falled to one pixel of matrix by one iteration"},
	// parameter with suboptions parsed directly in parseargs
	{"mir-parameters",NEED_ARG,NULL,'M',	arg_function,APTR(&get_mir_par),MirPar},
	// another variant of setting parameter without argument
	{"add-noice",NO_ARGS,	&G.randMask,1,	arg_none,	NULL,				"add random noice to mirror surface deviations"},
	{"force",	 NO_ARGS,	&rewrite_ifexists,1,arg_none,NULL,				"rewrite output file if exists"},
	// incremented parameter without args (any -v will increment value of "verbose")
	{"verbose",	NO_ARGS,	NULL,	'v',	arg_none,	APTR(&verbose),		"verbose level (each -v increase it)"},
	// integer parameter which can occur several times:
	{"dev-size",MULT_PAR,	NULL,	'd',	arg_int,	APTR(&G.S_dev),		"size of initial array of surface deviations"},
	// double array
	{"noice-amp",MULT_PAR,	NULL,	'a',	arg_double,	APTR(&G.randAmp),	"amplitude of random noice (default: 1e-8)"},
	// string array
	{"str",		MULT_PAR,	NULL,	's',	arg_string,	APTR(&G.str),		"some string parameters (may meets many times)"},
	// suboptions array that will be parsed outside (the same as string array):
	{"filter",	MULT_PAR,	NULL,	'f',	arg_string,	APTR(&G.filter),	FilPar},
	end_option
};


// suboptions structure for get_mirpars
// in array last MUST BE {0,0,0}
typedef struct{
	double *val;	// pointer to result
	char *par;		// parameter name (CASE-INSENSITIVE!)
	bool isdegr;	// == TRUE if parameter is an angle in format "[+-][DDd][MMm][SS.S]"
} suboptions;

/**
 * Safely convert data from string to double
 *
 * @param num (o) - double number read from string
 * @param str (i) - input string
 * @return TRUE if success
 */
bool myatod(double *num, const char *str){
	double res;
	char *endptr;
	assert(str);
	res = strtod(str, &endptr);
	if(endptr == str || *str == '\0' || *endptr != '\0'){
		printf("Wrong double number format!");
		return FALSE;
	}
	*num = res;
	return TRUE;
}

/**
 * Convert string "[+-][DDd][MMm][SS.S]" into radians
 *
 * @param ang (o) - angle in radians or exit with help message
 * @param str (i) - string with angle
 * @return TRUE if OK
 */
bool get_radians(double *ret, char *str){
	double val = 0., ftmp, sign = 1.;
	char *ptr;
	assert(str);
	switch(*str){ // check sign
		case '-':
			sign = -1.;
		case '+':
			str++;
	}
	if((ptr = strchr(str, 'd'))){ // found DDD.DDd
		*ptr = 0; if(!myatod(&ftmp, str)) return FALSE;
		ftmp = fabs(ftmp);
		if(ftmp > 360.){
			printf("Degrees should be less than 360");
			return FALSE;
		}
		val += ftmp;
		str = ptr + 1;
	}
	if((ptr = strchr(str, 'm'))){ // found DDD.DDm
		*ptr = 0; if(!myatod(&ftmp, str)) return FALSE;
		ftmp = fabs(ftmp);
		val += ftmp / 60.;
		str = ptr + 1;
	}
	if(strlen(str)){ // there is something more
		if(!myatod(&ftmp, str)) return FALSE;
		ftmp = fabs(ftmp);
		val += ftmp / 3600.;
	}
	*ret = D2R(val * sign); // convert degrees to radians
	return TRUE;
}

/**
 * Parse string of suboptions (--option=name1=var1:name2=var2... or -O name1=var1,name2=var2...)
 * Suboptions could be divided by colon or comma
 *
 * !!NAMES OF SUBOPTIONS ARE CASE-UNSENSITIVE!!!
 *
 * @param arg (i)    - string with description
 * @param V (io)     - pointer to suboptions array (result will be stored in sopts->val)
 * @return TRUE if success
 */
bool get_mirpars(void *arg, suboptions *V){
	char *tok, *val, *par;
	int i;
	tok = strtok(arg, ":,");
	do{
		if((val = strchr(tok, '=')) == NULL){ // wrong format
			printf("Wrong format: no value for keyword");
			return FALSE;
		}
		*val++ = '\0';
		par = tok;
		for(i = 0; V[i].val; i++){
			if(strcasecmp(par, V[i].par) == 0){ // found parameter
				if(V[i].isdegr){ // DMS
					if(!get_radians(V[i].val, val)) // wrong angle
						return FALSE;
				}else{ // simple double
					if(!myatod(V[i].val, val)) // wrong number
						return FALSE;
				}
				break;
			}
		}
		if(!V[i].val){ // nothing found - wrong format
			printf("Bad keyword!");
			return FALSE;
		}
	}while((tok = strtok(NULL, ":,")));
	return TRUE;
}

/**
 * functions of subargs parsing can looks as this
 */
/**
 * Parse string of mirror parameters (--mir-diam=...)
 *
 * @param arg (i) - string with description
 * @return TRUE if success
 */
bool get_mir_par(void *arg){
	suboptions V[] = { // array of mirror parameters and string keys for cmdln pars
		{&M.D,		"diam",		FALSE},
		{&M.F,		"foc",		FALSE},
		{&M.Zincl,	"zincl",	TRUE},
		{&M.Aincl,	"aincl",	TRUE},
		{0,0,0}
	};
	return get_mirpars(arg, V);
}

/**
 * Parse command line options and return dynamically allocated structure
 * 		to global parameters
 * @param argc - copy of argc from main
 * @param argv - copy of argv from main
 * @return allocated structure with global parameters
 */
glob_pars *parse_args(int argc, char **argv){
	int i;
	void *ptr;
	ptr = memcpy(&G, &Gdefault, sizeof(G)); assert(ptr);
	ptr = memcpy(&M, &Mdefault, sizeof(M)); assert(ptr);
	G.Mirror = &M;
	// format of help: "Usage: progname [args]\n"
	change_helpstring("Usage: %s [args]\n\n\tWhere args are:\n");
	// parse arguments
	parseargs(&argc, &argv, cmdlnopts);
	if(help) showhelp(-1, cmdlnopts);
	if(argc > 0){
		G.rest_pars_num = argc;
		G.rest_pars = calloc(argc, sizeof(char*));
		for (i = 0; i < argc; i++)
			G.rest_pars[i] = strdup(argv[i]);
	}
	return &G;
}

