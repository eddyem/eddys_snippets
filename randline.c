/*
 * randline.c - shows N lines with random number from choosen file
 *
 * COMPILE: gcc -lm -Wall -Werror -Wextra randline.c  -o randline
 *
 * Copyright 2014 Edward V. Emelianoff <eddy@sao.ru>
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

#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <limits.h>
#include <math.h>
#include <stddef.h>

#define ERR(...) do{fprintf(stderr, __VA_ARGS__); return NULL;}while(0)
#define _(s) s
#define MALLOC(type, size) ((type *)my_alloc(size, sizeof(type)))

// mmap file
typedef struct{
	char *data;
	size_t len;
} mmapbuf;

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
/*
 * Generate a quasy-random number to initialize PRNG
 * name: throw_random_seed
 * @return value for srand48
 */
long throw_random_seed(){
	long r_ini;
	int fail = 0;
	int fd = open("/dev/random", O_RDONLY);
	do{
		if(-1 == fd){
			perror(_("Can't open /dev/random"));
			fail = 1; break;
		}
		if(sizeof(long) != read(fd, &r_ini, sizeof(long))){
			perror(_("Can't read /dev/random"));
			fail = 1;
		}
		close(fd);
	}while(0);
	if(fail){
		double tt = dtime() * 1e6;
		double mx = (double)LONG_MAX;
		r_ini = (long)(tt - mx * floor(tt/mx));
	}
	return (r_ini);
}


void* my_alloc(size_t N, size_t S){
	void *p = calloc(N, S);
	if(!p){
		perror(_("can't allocate memory"));
		exit(-1);
	}
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
	if(!filename) ERR(_("No filename given!"));
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
		fprintf(stderr, _("Can't munmap"));
	free(b);
	b = NULL;
}

int main(int argc, char **argv){
	char *endptr, *buf, *startptr;
	mmapbuf *mbuf;
	ptrdiff_t MAX;
	size_t N_strings;
	void usage(){
		fprintf(stderr, _("USAGE: %s filename Nlines\n"), argv[0]);
	}
	if(argc != 3){
		usage();
		return -1;
	}
	if(!(mbuf = My_mmap(argv[1]))) return -2;
	MAX = mbuf->len;
	buf = mbuf->data;
	N_strings = strtol(argv[2], &endptr, 0);
	if(endptr == argv[2] || *endptr != '\0'){
		usage();
		return -3;
	}
	srand48(throw_random_seed());
	while(N_strings--){
		size_t R = lrand48() % MAX;
		startptr = strchr(buf+R, '\n');
		if(!startptr || ((++startptr-buf) >= MAX)){
			N_strings++;
			continue;
		}
		endptr = strchr(startptr, '\n');
		if(!endptr || endptr <= startptr){
			N_strings++;
			continue;
		}
		write(1, startptr, endptr-startptr);
		write(1, "\n", 1);
	}
	My_munmap(mbuf);
	return 0;
}


