/*
 * bidirectional_list.c - bidi sorted list
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

#include "bidirectional_list.h"

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


List *l_search(List *root, int v, List **Left, List **Right){
	List *L = NULL, *R = NULL;
	int dir = -1;
	if(!root) goto not_Fnd;
	if(root->data == v)
	return root;
	if(root->data < v) dir = 1;
	do{
		L = root->left_p;  // left leaf
		R = root->right_p; // right leaf
		int D = root->data;// data in leaf
		if(D > v && dir == -1){ // search to left is true
			R = root;
			root = L;
		}
		else if(D < v && dir == 1){ // search to right is true
			L = root;
			root = R;
		}
		else if(D == v){ // we found exact value
			if(Left)  *Left  = L;
			if(Right) *Right = R;
			return root;
		}
		else break;
	}while(root); // if root == NULL, we catch an edge
	// exact value not found
	if(root){ // we aren't on an edge
		if(dir == -1) L = root;
		else R = root;
	}
not_Fnd:
	if(Left)  *Left  = L;
	if(Right) *Right = R;
	return NULL; // value not found
}


List *l_insert(List *root, int v){
	List *node, *L, *R;
	node = l_search(root, v, &L, &R);
	if(node) return node; // we found exact value V
	// there's no exact value: insert new
	if((node = (List*) Malloc(1, sizeof(List))) == 0)  return NULL; // allocation error
	node->data = v; // insert data
	// add neighbours:
	node->left_p = L;
	node->right_p = R;
	// fix neighbours:
	if(L) L->right_p = node;
	if(R) R->left_p = node;
	return node;
}

void l_remove(List **root, int v){
	List *node, *left, *right;
	if(!root)  return;
	if(!*root) return;
	node = l_search(*root, v, &left, &right);
	if(!node) return; // there's no such element
	if(node == *root) *root = node->right_p;
	if(!*root) *root = node->left_p;
	FREE(node);
	if(left)  left->right_p = right;
	if(right) right->left_p = left;
}

#ifdef STANDALONE
int main(void) {
	List *tp, *root_p = NULL;
	int i, ins[] = {4,2,6,1,3,4,7};
	for(i = 0; i < 7; i++)
	if(!(root_p = l_insert(root_p, ins[i])))
		err(1, "Malloc error"); // can't insert
	for(i = 0; i < 9; i++){
		tp = l_search(root_p, i, NULL, NULL);
		printf("%d ", i);
		if(!tp) printf("not ");
		printf("found\n");
	}
	printf("now delete items 1, 4 and 8\n");
	l_remove(&root_p, 1);
	l_remove(&root_p, 4);
	l_remove(&root_p, 8);
	for(i = 0; i < 9; i++){
		tp = l_search(root_p, i, NULL, NULL);
		printf("%d ", i);
		if(!tp) printf("not ");
		printf("found\n");
	}
	return 0;
}
#endif
