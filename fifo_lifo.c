/*
 * fifo_lifo.c - simple FIFO/LIFO
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
#include <stdio.h>
#include <stdlib.h>
#include <err.h>

#include "fifo_lifo.h"

#ifndef Malloc
#define Malloc my_alloc
/**
 * Memory allocation with control
 * @param N - number of elements
 * @param S - size of one element
 * @return allocated memory or die
 */
void *my_alloc(size_t N, size_t S){
    void *p = calloc(N, S);
    if(!p) err(1, "malloc");
    return p;
}
#endif
#ifndef FREE
#define FREE(arg) do{free(arg); arg = NULL;}while(0)
#endif


/**
 * push data into the tail of a stack (like FIFO)
 * @param lst (io) - list
 * @param v (i)    - data to push
 * @return pointer to just pused node
 */
List *push_tail(List **lst, Ldata v){
	List *node;
	if(!lst) return NULL;
	if((node = (List*) Malloc(1, sizeof(List))) == 0)
		return NULL; // allocation error
	node->data = v; // insert data
	if(!*lst){
		*lst = node;
		(*lst)->last = node;
		return node;
	}
	(*lst)->last->next = node;
	(*lst)->last = node;
	return node;
}

/**
 * push data into the head of a stack (like LIFO)
 * @param lst (io) - list
 * @param v (i)    - data to push
 * @return pointer to just pused node
 */
List *push(List **lst, Ldata v){
	List *node;
	if(!lst) return NULL;
	if((node = (List*) Malloc(1, sizeof(List))) == 0)
		return NULL; // allocation error
	node->data = v; // insert data
	if(!*lst){
		*lst = node;
		(*lst)->last = node;
		return node;
	}
	node->next = *lst;
	node->last = (*lst)->last;
	*lst = node;
	return node;
}

/**
 * get data from head of list
 * @param lst (io) - list
 * @return data from lst head
 */
Ldata pop(List **lst){
	Ldata ret;
	List *node = *lst;
	if(!lst) errx(1, "NULL pointer to buffer");
	if(!*lst) errx(1, "Empty buffer");
	ret = (*lst)->data;
	*lst = (*lst)->next;
	FREE(node);
	return ret;
}

#ifdef STANDALONE
int main(void) {
	List *f = NULL;
	int i, d, ins[] = {4,2,6,1,3,4,7};
	for(i = 0; i < 7; i++)
		if(!(push(&f, ins[i])))
			err(1, "Malloc error"); // can't insert
	else printf("pushed %d\n", ins[i]);
	while(f){
		d = pop(&f);
		printf("pulled: %d\n", d);
	}
	for(i = 0; i < 7; i++)
		if(!(push_tail(&f, ins[i])))
			err(1, "Malloc error"); // can't insert
	else printf("pushed to head %d\n", ins[i]);
	while(f){
		d = pop(&f);
		printf("pulled: %d\n", d);
	}
	return 0;
}
#endif
