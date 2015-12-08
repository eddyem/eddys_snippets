/*
 * getopt.c - simple functions for getopt_long
 * provide functions
 * 		usage      - to show help message & exit
 *		parse_args - to parce argv & fill global flags
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
#include <getopt.h>
#include <stdarg.h>


/*
 * here are global variables and global data structures initialisation, like
 * int val = 10; // default value
 */

/*
 * HERE A PART of main.c:
 *
	setlocale(LC_ALL, "");
	setlocale(LC_NUMERIC, "C");
	bindtextdomain(GETTEXT_PACKAGE, LOCALEDIR);
	textdomain(GETTEXT_PACKAGE);
	parse_args(argc, argv); // <- set all flags
 *
 * ITS HEADER:
#ifndef GETTEXT_PACKAGE // should be defined by make
#define GETTEXT_PACKAGE "progname"
#endif
#ifndef LOCALEDIR // should be defined by make
#define LOCALEDIR "/path/to/locale"
#endif
 */

void usage(char *fmt, ...){
	va_list ap;
	va_start(ap, fmt);
	printf("\n");
	if (fmt != NULL){
		vprintf(fmt, ap);
		printf("\n\n");
	}
	va_end(ap);
	// "Использование:\t%s [опции] [префикс выходных файлов]\n"
	printf(_("Usage:\t%s [options] [output files prefix]\n"),
		__progname);
	// "\tОпции:\n"
	printf(_("\tOptions:\n"));
	printf("\t-A,\t--author=author\t\t%s\n",
		// "автор программы"
		_("program author"));
	// and so on
	exit(1);
}

void parse_args(int argc, char **argv){
	int i;
	char short_options[] = "A"; // all short equivalents
	struct option long_options[] = {
/*		{ name, has_arg, flag, val }, where:
 * name - name of long parameter
 * has_arg = 0 - no argument, 1 - need argument, 2 - unnesessary argument
 * flag = NULL to return val, pointer to int - to set it
 * 		value of val (function returns 0)
 * val -  getopt_long return value or value, flag setting to
 * !!! last string - for zeros !!!
 */
		{"author",		1,	0,	'A'},
		// and so on
		{ 0, 0, 0, 0 }
	};
	if(argc == 1){
		// "Не введено никаких параметров"
		usage(_("Any parameters are absent"));
	}
	while(1){
		int opt;
		if((opt = getopt_long(argc, argv, short_options,
			long_options, NULL)) == -1) break;
		switch(opt){
		case 0: // only long option
			// do something?
		break;
		case 'A':
			author = strdup(optarg);
			// "Автор программы: %s"
			//info(_("Program author: %s"), author);
			break;
		// ...
		default:
		usage(NULL);
		}
	}
	argc -= optind;
	argv += optind;
	if(argc == 0){
		// there's no free parameters
	}else{
		/* there was free parameter
		 * for example, filename
		outfile = argv[0];
		argc--;
		argv++;
		*/
	}
	if(argc > 0){
		// "Игнорирую аргумент[ы]:\n"
		printf(_("Ignore argument[s]:\n"));
		for (i = 0; i < argc; i++)
			warnx("%s ", argv[i]);
	}
}
