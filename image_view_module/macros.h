/*
 * macros.h
 *
 * Copyright 2015 Edward V. Emelianov <eddy@sao.ru, edward.emelianoff@gmail.com>
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
#ifndef __MACROS_H__
#define __MACROS_H__

#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include <err.h>
#include <locale.h>
#include <libintl.h>
#include <stdlib.h>
#include <termios.h>
#include <termio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <stdint.h>
#include <pthread.h>



// not used args
#define _U_    __attribute__((__unused__))

#define check_ptr(ptr, fn) \
	if((ptr) == NULL) { \
		printf("%10s: NULL pointer\n",(char *)(fn)); \
		perror((char *)(fn)); \
		exit(1); \
	}

#ifdef NOGETTEXT
	#ifdef gettext
		#undef gettext
	#endif
	#define gettext(arg)  arg
#endif
#define _(String)				gettext(String)
#define gettext_noop(String)	String
#define N_(String)				gettext_noop(String)

/*
 * Coloured messages output
 */
#define RED			"\033[1;31;40m"
#define GREEN		"\033[1;32;40m"
#define OLDCOLOR	"\033[0;0;0m"

extern int globErr;

#define ERR(...) do{globErr=errno; _WARN(__VA_ARGS__); exit(-1);}while(0)
#define ERRX(...) do{globErr=0; _WARN(__VA_ARGS__); exit(-1);}while(0)
#define WARN(...) do{globErr=errno; _WARN(__VA_ARGS__);}while(0)
#define WARNX(...) do{globErr=0; _WARN(__VA_ARGS__);}while(0)

/*
 * print function name, debug messages
 * debug mode, -DEBUG
 */
#ifdef EBUG
	#define FNAME() fprintf(stderr, "\n%s (%s, line %d)\n", __func__, __FILE__, __LINE__)
	#define DBG(...) do{fprintf(stderr, "%s (%s, line %d): ", __func__, __FILE__, __LINE__); \
					fprintf(stderr, __VA_ARGS__);			\
					fprintf(stderr, "\n");} while(0)
#else
	#define FNAME()	 do{}while(0)
	#define DBG(...) do{}while(0)
#endif //EBUG

/*
 * Memory allocation
 */
#define ALLOC(type, var, size)  type * var = ((type *)my_alloc(size, sizeof(type)))
#define MALLOC(type, size) ((type *)my_alloc(size, sizeof(type)))
#define FREE(ptr)			do{free(ptr); ptr = NULL;}while(0)

// functions for color output in tty & no-color in pipes
extern int (*red)(const char *fmt, ...);
extern int (*_WARN)(const char *fmt, ...);
extern int (*green)(const char *fmt, ...);
void * my_alloc(size_t N, size_t S);
void initial_setup();

// mmap file
typedef struct{
	char *data;
	size_t len;
} mmapbuf;
mmapbuf *My_mmap(char *filename);
void My_munmap(mmapbuf *b);

#endif // __MACROS_H__
