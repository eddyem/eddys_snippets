/*
 * copy_pcb.c - copy kicad PCBs
 *
 * Copyright 2014 Edward V. Emelianov <eddy@sao.ru, edward.emelianoff@gmail.com>
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

char **List;

/**
 * Printout string from ptr0 (including) to ptr1 (excluding) and return ptr1
 */
char *printdata(char *ptr0, char *ptr1){
	char *ptr = ptr0;
	while(ptr < ptr1) printf("%c", *ptr++);
	return ptr1;
}

int build_list(){
	char *tok;
	int L, i;
	tok = G.list;
	for(L = 0; strchr(tok, ','); L++, tok++);
	DBG("found: %d commas\n", L);
	if(G.nparts > 0 && G.nparts < L) L = G.nparts;
	List = MALLOC(char*, L);
	tok = strtok(G.list, ",");
	if(!tok) return 0;
	i = 0;
	do{
		List[i++] = strdup(tok);
	}while((tok = strtok(NULL, ",")) && i < L);
	return i;
}

int main(int argc, char **argv){
	int i, j, L;
	mmapbuf *input;
	initial_setup();
	parce_args(argc, argv);
	if(!G.ifile){
		ERRX("You shold give input file name\n");
		return(-1);
	}
	if(G.format) DBG("Format: %s\n", G.format);
	else{
		DBG("No format defined, use \"%%s.%%d\"\n");
		G.format = "%s.%d";
	}
	if(G.nparts) DBG("Use first %d copies of list\n", G.nparts);
	else DBG("Use all list given\n");
	if(!G.list){
		WARNX("Error! You should say\n\t\t%s -L list\n", __progname);
		ERR("\twhere \"list\" is a comma-separated list of sheets\n");
		return(-1);
	}
	L = build_list();
	DBG("Found %d sheets in list:\n", L);
	if(L == 0){
		ERRX("Error: empty list\n");
		return(-1);
	}
	for(i = 0; i < L; i++) DBG("\titem %d: %s\n", i, List[i]);
	if(!(input = My_mmap(G.ifile))){
		ERRX("A strange error: can't mmap input file\n");
		return(-1);
	}
	char *iptr = input->data, *oldptr = iptr;

	i = 0;
	char comp[32], val[32];
	int N1, N2;
	while((iptr = strstr(iptr, "$Comp\n"))){
		DBG("%dth component: ", ++i);
		iptr = strstr(iptr, "\nL ");
		if(!iptr) break; iptr += 3;
		if(sscanf(iptr, "%s %s\n", comp, val) != 2) continue;
		DBG("component %s with label %s\n", comp, val);
		iptr = strstr(iptr, "\nU ");
		if(!iptr) break; iptr += 3;
		if(sscanf(iptr, "%d %d %s\n",&N1,&N2,comp) != 3) continue;
		DBG("N1 = %d; N2 = %d; comp label: %s\n",N1,N2,comp);
		iptr = strstr(iptr, "\nF"); // go to line "F 0"
		if(!iptr) break; iptr++;
		// printout all what was before:
		oldptr = printdata(oldptr, iptr);
		for(j = 0; j < L; j++){
			printf("AR Path=\"/%s/%s\" Ref=\"", List[j], comp);
			printf(G.format, val, j+1);
			printf("\"  Part=\"1\"\n");
		}
	}
	// printout the rest of file
	if(!iptr) iptr = input->data + input->len;
	printdata(oldptr, iptr);
	My_munmap(input);
	return 0;
}

