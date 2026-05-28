/*
 * fifo_lifo.h
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
#ifndef __FIFO_LIFO_H__
#define __FIFO_LIFO_H__

typedef int Ldata;

typedef struct buff_node{
	Ldata data;
	struct buff_node *next, *last;
} List;

List *push_tail(List **lst, Ldata v);
List *push(List **lst, Ldata v);
Ldata pop(List **lst);

#endif // __FIFO_LIFO_H__
