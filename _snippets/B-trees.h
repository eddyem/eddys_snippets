/*
 * B-trees.h
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

#pragma once
#ifndef __B_TREES_H__
#define __B_TREES_H__

typedef struct btree_node{
	int data;
	struct btree_node *left, *right;
} BTree;

BTree *bt_get(BTree *root, int v);
BTree *bt_search(BTree *root, int v, BTree **prev);
BTree *bt_create_leaf(int v);
BTree *bt_insert(BTree **root, int v);
BTree *max_leaf(BTree *root, BTree **prev);
BTree *min_leaf(BTree *root, BTree **prev);
void bt_remove(BTree **root, int v);

void print_tree(BTree *leaf);

#endif // __B_TREES_H__
