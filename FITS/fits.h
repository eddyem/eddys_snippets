/*
 * fits.h
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
#pragma once
#ifndef __FITS_H__
#define __FITS_H__

#include <stdio.h>
#include <stdbool.h>


typedef struct{
	int width;			// width
	int height;			// height
	int dtype;			// data type
	double *data;		// picture data
	char **keylist;		// list of options for each key
	size_t keynum;		// full number of keys (size of *keylist)
} IMAGE;

void imfree(IMAGE **ima);
bool readFITS(char *filename, IMAGE **fits);
bool writeFITS(char *filename, IMAGE *fits);

#endif // __FITS_H__
