/*
 * simple_list.c - simple one-direction list
 *
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
#include <string.h>
#include <err.h>

#include "simple_list.h"

#define MALLOC alloc_simple_list
/**
 * Memory allocation with control
 * @param N - number of elements
 * @param S - size of one element
 * @return allocated memory or die
 */
static void *alloc_simple_list(size_t N, size_t S){
    void *p = calloc(N, S);
    if(!p) err(1, "malloc");
    return p;
}

void (*listdata_free)(void *node_data) = NULL; // external function to free(listdata)

#define FREE(arg) do{free(arg); arg = NULL;}while(0)

/**
 * add element v to list with root root (also this can be last element)
 * @param root (io) - pointer to root (or last element) of list or NULL
 * 						if *root == NULL, just created node will be placed there
 * @param v         - data inserted
 * @return pointer to created node
 */
List *list_add(List **root, void *v){
	List *node, *last;
	if((node = (List*) MALLOC(1, sizeof(List))) == 0)  return NULL; // allocation error
	node->data = v; // insert data
	if(root){
		if(*root){ // there was root node - search last
			last = *root;
			while(last->next) last = last->next;
			last->next = node; // insert pointer to new node into last element in list
		}
		if(!*root) *root = node;
	}
	return node;
}

/**
 * set listdata_free()
 * @param fn - function that freec memory of (node)
 */
void listfree_function(void (*fn)(void *node)){
	listdata_free = fn;
}

/**
 * remove all nodes in list
 * @param root - pointer to root node
 */
void list_free(List **root){
	List *node = *root, *next;
	if(!root || !*root) return;
	do{
		next = node->next;
		if(listdata_free)
			listdata_free(node->data);
		free(node);
		node = next;
	}while(node);
	*root = NULL;
}

