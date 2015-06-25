/*
 * spots.h
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
#ifndef __SPOTS_H__
#define __SPOTS_H__

#include <stdint.h>
#include "zernike.h"
#include "simple_list.h"

// focal ratio of BTA in millimeters
#define FOCAL_R     (24024)
// BTA primary mirror radius (in mm)
#define MIR_R       (3025)
// Distance from mirror to hartmann mask
#define HARTMANN_Z  (20017)

#define _(...) __VA_ARGS__
#define MirRadius 3.  // main mirror radius

extern double pixsize;
extern double distance;

/*
// spot center
typedef struct{
	int id;		// spot identificator
	double x;	// coordinates of center
	double y;
} spot;
*/

// hartmannogram
typedef struct{
	char *filename;   // spots-filename
	uint8_t got[258]; // == 1 if there's this spot on image
	point center;     // coordinate of center
	point spots[258]; // spotlist, markers have numbers 256 & 257
} hartmann;

// mirror
typedef struct{
	uint8_t got[258]; // == 1 if there's this spot on both pre-focal & post-focal images
	int spotsnum;     // total amount of spots used
	point center;     // x&y coordinates of center beam
	point tanc;       // tangent of center beam
	point tans[258];  // tangents of beams == (r_post-r_pre)/d
	point spots[258]; // x&y coordinates of spots on mirror surface (z=[x^2+y^2]/4f)
	polar pol_spots[258]; // polar coordinates of spots on mirror surface according to center (norm by mirror R)
	point grads[258]; // gradients to mirror surface
	double zbestfoc;  // Z-coordinate (from pre-focal image) of best focus for minimal circle of confusion
	double z07;       // Z of best focus for q=0.7
//	double Rmax;      // max R of spot (@ which pol_spots.r == 1.)
} mirror;

// spot diagram
typedef struct{
	uint8_t got[258];   // the same as above
	point center;       // center beam's coordinate
	point spots[258];   // spots
	double z;           // z from prefocal image
}spot_diagram;

// gradients structure: point coordinates & gradient components
typedef struct{
	int id;
	double x;
	double y;
	double Dx;
	double Dy;
} CG;

hartmann *read_spots(char *filename, int prefocal);
void h_free(hartmann **H);
mirror *calc_mir_coordinates(hartmann *H[]);
void getQ(mirror *mir, hartmann *prefoc);
double calc_Hartmann_constant(mirror *mir, hartmann *H);
spot_diagram *calc_spot_diagram( mirror *mir, hartmann *H, double z);
void calc_gradients(mirror *mir, spot_diagram *foc_spots);

/*
size_t get_gradients(hartmann *H[], polar **coords, point **grads, double *scale);
size_t read_gradients(char *gradname, polar **coords, point **grads, double *scale);
*/
#endif // __SPOTS_H__
