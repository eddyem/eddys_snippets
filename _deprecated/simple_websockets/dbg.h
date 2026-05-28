/*
 * dbg.h
 *
 * Copyright 2016 Edward V. Emelianov <eddy@sao.ru, edward.emelianoff@gmail.com>
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
#ifndef __DBG_H__
#define __DBG_H__

#if defined EBUG || defined DEBUG
#ifndef DBG
#define DBG(...) do{fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n");}while(0)
#endif
#else
#define DBG(...)
#endif


#ifndef _U_
	#define _U_    __attribute__((__unused__))
#endif

#endif // __DBG_H__
