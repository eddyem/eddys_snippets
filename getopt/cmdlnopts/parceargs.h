/*
 * parceargs.h - headers for parcing command line arguments
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
#ifndef __PARCEARGS_H__
#define __PARCEARGS_H__

#include <stdbool.h>// bool
#include <stdlib.h>

#ifndef TRUE
	#define TRUE true
#endif

#ifndef FALSE
	#define FALSE false
#endif

// macro for argptr
#define APTR(x)   ((void*)x)

// if argptr is a function:
typedef  bool(*argfn)(void *arg);

/*
 * type of getopt's argument
 * WARNING!
 * My function change value of flags by pointer, so if you want to use another type
 * make a latter conversion, example:
 * 		char charg;
 * 		int iarg;
 * 		myoption opts[] = {
 * 		{"value", 1, NULL, 'v', arg_int, &iarg, "char val"}, ..., end_option};
 * 		..(parce args)..
 * 		charg = (char) iarg;
 */
typedef enum {
	arg_none = 0,	// no arg
	arg_int,		// integer
	arg_longlong,	// long long
	arg_double,		// double
	arg_float,		// float
	arg_string,		// char *
	arg_function	// parce_args will run function `bool (*fn)(char *optarg, int N)`
} argtype;

/*
 * Structure for getopt_long & help
 * BE CAREFUL: .argptr is pointer to data or pointer to function,
 * 		conversion depends on .type
 *
 * ATTENTION: string `help` prints through macro PRNT(), bu default it is gettext,
 * but you can redefine it before `#include "parceargs.h"`
 *
 * if arg is string, then value wil be strdup'ed like that:
 * 		char *str;
 * 		myoption opts[] = {{"string", 1, NULL, 's', arg_string, &str, "string val"}, ..., end_option};
 *		*(opts[1].str) = strdup(optarg);
 * in other cases argptr should be address of some variable (or pointer to allocated memory)
 *
 * NON-NULL argptr should be written inside macro APTR(argptr) or directly: (void*)argptr
 *
 * !!!LAST VALUE OF ARRAY SHOULD BE `end_option` or ZEROS !!!
 *
 */
typedef enum{
	NO_ARGS  = 0,  // first three are the same as in getopt_long
	NEED_ARG = 1,
	OPT_ARG  = 2,
	MULT_PAR
} hasarg;

typedef struct{
	// these are from struct option:
	const char *name;		// long option's name
	hasarg      has_arg;	// 0 - no args, 1 - nesessary arg, 2 - optionally arg, 4 - need arg & key can repeat (args are stored in null-terminated array)
	int        *flag;		// NULL to return val, pointer to int - to set its value of val (function returns 0)
	int         val;		// short opt name (if flag == NULL) or flag's value
	// and these are mine:
	argtype     type;		// type of argument
	void       *argptr;		// pointer to variable to assign optarg value or function `bool (*fn)(char *optarg, int N)`
	const char *help;		// help string which would be shown in function `showhelp` or NULL
} myoption;

/*
 * Suboptions structure, almost the same like myoption
 * used in parce_subopts()
 */
typedef struct{
	const char *name;
	hasarg      has_arg;
	argtype     type;
	void       *argptr;
} mysuboption;

// last string of array (all zeros)
#define end_option {0,0,0,0,0,0,0}
#define end_suboption {0,0,0,0}

extern const char *__progname;

void showhelp(int oindex, myoption *options);
void parceargs(int *argc, char ***argv, myoption *options);
void change_helpstring(char *s);
bool get_suboption(char *str, mysuboption *opt);

#endif // __PARCEARGS_H__
