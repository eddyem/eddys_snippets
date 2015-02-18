/*
 * usefull_macros.h - a set of usefull functions: memory, color etc
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

#include "macros.h"


#include <sys/time.h>
/**
 * function for different purposes that need to know time intervals
 * @return double value: time in seconds
 */
double dtime(){
	double t;
	struct timeval tv;
	gettimeofday(&tv, NULL);
	t = tv.tv_sec + ((double)tv.tv_usec)/1e6;
	return t;
}

/******************************************************************************\
 *                          Coloured terminal
\******************************************************************************/
int globErr = 0; // errno for WARN/ERR

// pointers to coloured output printf
int (*red)(const char *fmt, ...);
int (*green)(const char *fmt, ...);
int (*_WARN)(const char *fmt, ...);

/*
 * format red / green messages
 * name: r_pr_, g_pr_
 * @param fmt ... - printf-like format
 * @return number of printed symbols
 */
int r_pr_(const char *fmt, ...){
	va_list ar; int i;
	printf(RED);
	va_start(ar, fmt);
	i = vprintf(fmt, ar);
	va_end(ar);
	printf(OLDCOLOR);
	return i;
}
int g_pr_(const char *fmt, ...){
	va_list ar; int i;
	printf(GREEN);
	va_start(ar, fmt);
	i = vprintf(fmt, ar);
	va_end(ar);
	printf(OLDCOLOR);
	return i;
}
/*
 * print red error/warning messages (if output is a tty)
 * @param fmt ... - printf-like format
 * @return number of printed symbols
 */
int r_WARN(const char *fmt, ...){
	va_list ar; int i = 1;
	fprintf(stderr, RED);
	va_start(ar, fmt);
	if(globErr){
		errno = globErr;
		vwarn(fmt, ar);
		errno = 0;
		globErr = 0;
	}else
		i = vfprintf(stderr, fmt, ar);
	va_end(ar);
	i++;
	fprintf(stderr, OLDCOLOR "\n");
	return i;
}

const char stars[] = "****************************************";
/*
 * notty variants of coloured printf
 * name: s_WARN, r_pr_notty
 * @param fmt ... - printf-like format
 * @return number of printed symbols
 */
int s_WARN(const char *fmt, ...){
	va_list ar; int i;
	i = fprintf(stderr, "\n%s\n", stars);
	va_start(ar, fmt);
	if(globErr){
		errno = globErr;
		vwarn(fmt, ar);
		errno = 0;
		globErr = 0;
	}else
		i = +vfprintf(stderr, fmt, ar);
	va_end(ar);
	i += fprintf(stderr, "\n%s\n", stars);
	i += fprintf(stderr, "\n");
	return i;
}
int r_pr_notty(const char *fmt, ...){
	va_list ar; int i;
	i = printf("\n%s\n", stars);
	va_start(ar, fmt);
	i += vprintf(fmt, ar);
	va_end(ar);
	i += printf("\n%s\n", stars);
	return i;
}

/**
 * Run this function in the beginning of main() to setup locale & coloured output
 */
void initial_setup(){
	// setup coloured output
	if(isatty(STDOUT_FILENO)){ // make color output in tty
		red = r_pr_; green = g_pr_;
	}else{ // no colors in case of pipe
		red = r_pr_notty; green = printf;
	}
	if(isatty(STDERR_FILENO)) _WARN = r_WARN;
	else _WARN = s_WARN;
	// Setup locale
	setlocale(LC_ALL, "");
	setlocale(LC_NUMERIC, "C");
#ifndef NOGETTEXT
	bindtextdomain(GETTEXT_PACKAGE, LOCALEDIR);
	textdomain(GETTEXT_PACKAGE);
#endif
}

/******************************************************************************\
 *                                  Memory
\******************************************************************************/
/*
 * safe memory allocation for macro ALLOC
 * @param N - number of elements to allocate
 * @param S - size of single element (typically sizeof)
 * @return pointer to allocated memory area
 */
void *my_alloc(size_t N, size_t S){
	void *p = calloc(N, S);
	if(!p) ERR("malloc");
	//assert(p);
	return p;
}

/**
 * Mmap file to a memory area
 *
 * @param filename (i) - name of file to mmap
 * @return stuct with mmap'ed file or die
 */
mmapbuf *My_mmap(char *filename){
	int fd;
	char *ptr;
	size_t Mlen;
	struct stat statbuf;
	if(!filename) ERRX(_("No filename given!"));
	if((fd = open(filename, O_RDONLY)) < 0)
		ERR(_("Can't open %s for reading"), filename);
	if(fstat (fd, &statbuf) < 0)
		ERR(_("Can't stat %s"), filename);
	Mlen = statbuf.st_size;
	if((ptr = mmap (0, Mlen, PROT_READ, MAP_PRIVATE, fd, 0)) == MAP_FAILED)
		ERR(_("Mmap error for input"));
	if(close(fd)) ERR(_("Can't close mmap'ed file"));
	mmapbuf *ret = MALLOC(mmapbuf, 1);
	ret->data = ptr;
	ret->len = Mlen;
	return  ret;
}

void My_munmap(mmapbuf *b){
	if(munmap(b->data, b->len))
		ERR(_("Can't munmap"));
	FREE(b);
}

