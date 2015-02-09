/*
 * B-trees.c - my realisation of binary serch trees
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
#include <assert.h>
#include <stdint.h>

#include "B-trees.h"

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

#ifdef EBUG
#ifndef DBG
	#define DBG(...) do{fprintf(stderr, __VA_ARGS__);}while(0)
#endif
#else
#ifndef DBG
	#define DBG(...)
#endif
#endif

static inline uint32_t log_2(const uint32_t x) {
    if(x == 0) return 0;
    return (31 - __builtin_clz (x));
}

// later this function maybe mode difficult
void bt_destroy(BTree *node){
	FREE(node);
}

// get tree leaf nearest to v
BTree *bt_get(BTree *root, int v){
	if(!root) return NULL;
	int D = root->data;
	if(D == v) return root;
	if(root->data > v) return root->left;
	else return root->right;
}

// find leaf with v or nearest to it (prev)
// prev is nearest parent or NULL for root
BTree *bt_search(BTree *root, int v, BTree **prev){
	if(!root){
		if(prev) *prev = NULL;
		return NULL;
	}
	BTree *p;
	do{
		p = root;
		root = bt_get(root, v);
	}while(root && root->data != v);
	if(prev){
		if(p != root) *prev = p;
		else *prev = NULL;
	}
	return root;
}

BTree *bt_create_leaf(int v){
	BTree *node = Malloc(1, sizeof(BTree));
	if(!node) return NULL;
	node->data = v;
	return node;
}

// insert new leaf (or leave it as is, if exists)
// return new node
BTree *bt_insert(BTree **root, int v){
	BTree *closest = NULL, *node = NULL;
	if(root && *root){
		node = bt_search(*root, v, &closest);
		if(node) return node; // value exists
	}
	node = bt_create_leaf(v);
	if(!node) return NULL;
	if(!closest){ // there's no values at all!
		if(root) *root = node; // modify root node
		return node;
	}
	// we have a closest node to our data, so create node and insert it:
	int D = closest->data;
	if(D > v) // there's no left leaf of closest node, add our node there
		closest->left = node;
	else
		closest->right = node;
	return node;
}

// find mostly right element or NULL if no right children
BTree *max_leaf(BTree *root, BTree **prev){
	if(!root)
		return NULL;
	BTree *ret = root->left;
	if(prev) *prev = root;
	while(ret->right){
		if(prev) *prev = ret;
		ret = ret->right;
	}
	return ret;
}
// find mostly left element or NULL if no left children
BTree *min_leaf(BTree *root, BTree **prev){
	if(!root) return NULL;
	BTree *ret = root->right;
	if(prev) *prev = root;
	while(ret->left){
		if(prev) *prev = ret;
		ret = ret->left;
	}
	return ret;
}

void print_tree(BTree *leaf){
	if(!leaf){
		DBG("null");
		return;
	}
	DBG("(%d: ", leaf->data);
	print_tree(leaf->left);
	DBG(", ");
	print_tree(leaf->right);
	DBG(")");
}

// remove element, correct root if it is removed
void bt_remove(BTree **root, int v){
	assert(root);
	void conn(BTree *p, BTree *n, BTree *node){
	DBG("connect: %d and %d through %d\n", p ? p->data : -1,
			n? n->data : -1, node->data);
		if(p->left == node) p->left = n;
		else p->right = n;
	}
	void conn_node(BTree *prev, BTree *next, BTree *node){
		if(!prev) *root = next; // this is a root
		else conn(prev, next, node);
		bt_destroy(node);
	}
	void node_subst(BTree *nold, BTree *nnew, BTree *pold, BTree *pnew){
		DBG("Substitute: %d by %d\n", nold-> data, nnew->data);
		if(!pold) *root = nnew; // this is a root
		else conn(pold, nnew, nold); // connect parent of old element to new
		conn(pnew, NULL, nnew); // remove node from it's parent
		nnew->left = nold->left;
		nnew->right = nold->right;
		bt_destroy(nold);
	}
	BTree *node, *prev, *Lmax, *Rmin, *Lmp, *Rmp;
	int L, R;
	node = bt_search(*root, v, &prev);
	if(!node) return; // there's no node v
	if(!node->left){ // there's only right child or none at all
		conn_node(prev, node->right, node);
		return;
	}
	if(!node->right){ // the same but like mirror
		conn_node(prev, node->left, node);
		return;
	}
	// we have situation that element have both children
	// 1) find max from left child and min from right
	Lmax = max_leaf(node, &Lmp);
	L = Lmax->data;
	Rmin = min_leaf(node, &Rmp);
	R = Rmin->data;
	// 2) now L is left max, R - right min
	// we select the nearest to v
	if(v - L > R - v){ // R is closest value, substitute node by Rmin
		node_subst(node, Rmin, prev, Rmp);
	}else{ // substitute node by Lmax
		node_subst(node, Lmax, prev, Lmp);
	}
}

// return node need rotation
BTree *rotate_ccw(BTree *node, BTree *prev){
	if(!node) return NULL;
	#ifdef EBUG
	DBG("\n\ntry to rotate near %d", node->data);
	if(prev) DBG(", right element: %d", prev->data);
	if(!node->right) DBG("\n");
	#endif
	if(!node->right) return node->left; // no right child -> return left
	BTree *chld = node->right;
	DBG(", child: %d\n", chld->data);
	if(prev) prev->left = chld;
	node->right = chld->left;
	chld->left = node;
	if(chld->right){DBG("R:%d\n",chld->right->data); return chld;} // need another rotation
	else return node;
}

void tree_to_vine(BTree **root){
	assert(root); assert(*root);
	BTree *node = *root, *prev;
	while((*root)->right) *root = (*root)->right; // max element will be a new root
	while(node && node->right)
		node = rotate_ccw(node, NULL); // go to new root
	prev = node; node = prev->left;
	while(rotate_ccw(node, prev)){
		node = prev->left;
		if(!node->right){
			prev = node;
			node = node->left;
		}
	}
}

// return node->left
BTree *rotate_cw(BTree *node, BTree *parent){ // rotate cw near node->left
	if(!node) return NULL;
	DBG("\n\ntry to rotate cw near %d\n", node->data);
	#ifdef EBUG
	if(parent)DBG("\t\tparent: %d\n", parent->data);
	#endif
	if(!node->left) return NULL;
	BTree *chld = node->left, *rght = chld->right;
	if(parent) parent->left = chld;
	DBG("\t\t, child: %d\n", chld->data);
	chld->right = node;
	node->left = rght;
	return chld;
}

// make balanced tree
void vine_to_tree(BTree **root){
	uint32_t depth, i;
	BTree *node = *root, *prev;
	for(depth = 0; node; depth++, node = node->left);
	DBG("depth: %u; ", depth);
	depth = log_2(depth);
	DBG("log: %d\n", depth);
	for(i = 0; i < depth; i++){
		DBG("\nIteration %d\n", i);
		node = *root;
		prev = NULL;
		if((*root)->left) *root = (*root)->left;
		while(node && node->left){
			node = rotate_cw(node, prev);
			if(!node) break;
			prev = node;
			node = node->left;
		}
	}
}

#ifdef STANDALONE
#define INS(X)  do{if(!bt_insert(&root, X)) err(1, "alloc error");}while(0)
int main(void){
	BTree *tp, *root = NULL;
	int i, ins[] = {7,3,12,1,5,9,20,0,4,6,13,21};
	void search_bt(){
		printf("\nRoot: %d\nTree", root->data);
		print_tree(root); printf("\n\n");
		for(i = 0; i < 22; i++){
			tp = bt_search(root, i, NULL);
			printf("%d ", i);
			if(!tp) printf("not ");
			printf("found\n");
		}
	}
	for(i = 0; i < 12; i++)
		INS(ins[i]);

	search_bt();
	printf("now we delete leafs 4, 12 and 7 (old root!)\n");
	bt_remove(&root, 4); bt_remove(&root, 12); bt_remove(&root, 7);
	search_bt();
	printf("add items 2, 4, 19\n");
	INS(2); INS(4); INS(19);
	search_bt();
	printf("\nnow make a vine\n");
	tree_to_vine(&root);
	search_bt();
	printf("\nAnd balance tree\n");
	vine_to_tree(&root);
	search_bt();
	return 0;
}
#endif
