/*
 * comments_before.c - put multiline comments before text in string they follow
 *
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

#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

typedef struct{
	char *data;
	size_t len;
} mmapbuf;

mmapbuf *My_mmap(char *filename){
	int fd;
	char *ptr;
	size_t Mlen;
	struct stat statbuf;
	if(!filename) exit(-1);
	if((fd = open(filename, O_RDONLY)) < 0) exit(-2);
	if(fstat (fd, &statbuf) < 0) exit(-3);
	Mlen = statbuf.st_size;
	if((ptr = mmap (0, Mlen, PROT_READ, MAP_PRIVATE, fd, 0)) == MAP_FAILED)
		exit(-4);
	if(close(fd)) exit(-5);
	mmapbuf *ret = calloc(1, sizeof(mmapbuf));
	ret->data = ptr;
	ret->len = Mlen;
	return  ret;
}


int main(int argc, char **argv){
	mmapbuf *buf = My_mmap(argv[1]);
	char *afile = strdup(buf->data), *eptr = afile + buf->len;
	char *beforecom = NULL;
	while(afile < eptr){
		char *nl = strchr(afile, '\n');
		if(!nl){
			printf("%s\n", afile);
			return 0;
		}
		*nl++ = 0;
		char *commstart = strstr(afile, "/*");
		if(commstart){
			*commstart = 0;
			free(beforecom);
			beforecom = strdup(afile);
			commstart += 2;
			char *commend = NULL;
			if(*commstart) commend = strstr(commstart, "*/");
			if(commend){ // single comment
				*commend = 0;
				afile = commend + 2;
				printf("/* %s */\n", commstart);
				printf("%s%s\n", beforecom, afile);
				afile = nl;
			}else{ // multiline comment
				printf("/*%s\n", commstart);
				commend = strstr(nl, "*/");
				if(!commend){
					printf("%s*/\n", nl);
					return -1;
				}
				*commend = 0;
				afile = commend + 2;
				if(*afile == '\n') ++afile;
				printf("%s*/\n", nl);
			}
		}else{
			printf("%s\n", afile);
			afile = nl;
		}
	}
}
