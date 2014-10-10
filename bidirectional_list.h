/*
 * bidirectional_list.h
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
#ifndef __BIDIRECTIONAL_LIST_H__
#define __BIDIRECTIONAL_LIST_H__

typedef struct list_node{
	int data;
	struct list_node *left_p, *right_p;
} List;

List *l_search(List *root, int v, List **Left, List **Right);
List *l_insert(List *root, int v);
void l_remove(List **root, int v);

#endif // __BIDIRECTIONAL_LIST_H__
