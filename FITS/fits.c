/*
 * fits.c - cfitsio routines
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

#include <string.h>
#include <stdio.h>
#include <fitsio.h>

#include "fits.h"
#include "usefull_macros.h"

static int fitsstatus = 0;

/*
 * Macros for error processing when working with cfitsio functions
 */
#define TRYFITS(f, ...)							\
do{ fitsstatus = 0;								\
	f(__VA_ARGS__, &fitsstatus);				\
	if(fitsstatus){								\
		fits_report_error(stderr, fitsstatus);	\
		return FALSE;}							\
}while(0)
#define FITSFUN(f, ...)							\
do{ fitsstatus = 0;								\
	int ret = f(__VA_ARGS__, &fitsstatus);		\
	if(ret || fitsstatus)						\
		fits_report_error(stderr, fitsstatus);	\
}while(0)
#define WRITEKEY(...)							\
do{ fitsstatus = 0;								\
	fits_write_key(__VA_ARGS__, &fitsstatus);	\
	if(status) fits_report_error(stderr, status);\
}while(0)

void imfree(IMAGE **img){
	size_t i, sz = (*img)->keynum;
	char **list = (*img)->keylist;
	for(i = 0; i < sz; ++i) FREE(list[i]);
	FREE((*img)->keylist);
	FREE((*img)->data);
	FREE(*img);
}

bool readFITS(char *filename, IMAGE **fits){
	FNAME();
	bool ret = TRUE;
	fitsfile *fp;
//	float nullval = 0., imBits, bZero = 0., bScale = 1.;
	int i, j, hdunum, hdutype, nkeys, keypos;
	int naxis;
	long naxes[2];
	char card[FLEN_CARD];
	IMAGE *img = MALLOC(IMAGE, 1);

	TRYFITS(fits_open_file, &fp, filename, READONLY);
	FITSFUN(fits_get_num_hdus, fp, &hdunum);
	if(hdunum < 1){
		WARNX(_("Can't read HDU"));
		ret = FALSE;
		goto returning;
	}
	// get image dimensions
	TRYFITS(fits_get_img_param, fp, 2, &img->dtype, &naxis, naxes);
	if(naxis > 2){
		WARNX(_("Images with > 2 dimensions are not supported"));
		ret = FALSE;
		goto returning;
	}
	img->width = naxes[0];
	img->height = naxes[1];
	DBG("got image %ldx%ld pix, bitpix=%d", naxes[0], naxes[1], img->dtype);
	// loop through all HDUs
	for(i = 1; !(fits_movabs_hdu(fp, i, &hdutype, &fitsstatus)); ++i){
		TRYFITS(fits_get_hdrpos, fp, &nkeys, &keypos);
		int oldnkeys = img->keynum;
		img->keynum += nkeys;
		if(!(img->keylist = realloc(img->keylist, sizeof(char*) * img->keynum))){
			ERR(_("Can't realloc"));
		}
		char **currec = &(img->keylist[oldnkeys]);
		DBG("HDU # %d of %d keys", i, nkeys);
		for(j = 1; j <= nkeys; ++j){
			FITSFUN(fits_read_record, fp, j, card);
			if(!fitsstatus){
				*currec = MALLOC(char, FLEN_CARD);
				memcpy(*currec, card, FLEN_CARD);
				DBG("key %d: %s", oldnkeys + j, *currec);
				++currec;
			}
		}
	}
	if(fitsstatus == END_OF_FILE){
		fitsstatus = 0;
	}else{
		fits_report_error(stderr, fitsstatus);
		ret = FALSE;
		goto returning;
	}
	size_t sz = naxes[0] * naxes[1];
	img->data = MALLOC(double, sz);
	int stat = 0;
	TRYFITS(fits_read_img, fp, TDOUBLE, 1, sz, NULL, img->data, &stat);
	if(stat) WARNX(_("Found %d pixels with undefined value"), stat);
	DBG("ready");

returning:
	FITSFUN(fits_close_file, fp);
	if(!ret){
		imfree(&img);
	}
	if(fits) *fits = img;
	return ret;
}

bool writeFITS(char *filename, IMAGE *fits){
	int w = fits->width, h = fits->height;
	long naxes[2] = {w, h};
	size_t sz = w * h, keys = fits->keynum;
	fitsfile *fp;

	TRYFITS(fits_create_file, &fp, filename);
	TRYFITS(fits_create_img, fp, fits->dtype, 2, naxes);

	if(keys){ // there's keys
		size_t i;
		char **records = fits->keylist;
		for(i = 0; i < keys; ++i){
			char *rec = records[i];
			if(strncmp(rec, "SIMPLE", 6) == 0 || strncmp(rec, "EXTEND", 6) == 0) // key "file does conform ..."
				continue;
			else if(strncmp(rec, "COMMENT", 7) == 0) // comment of obligatory key in FITS head
				continue;
			else if(strncmp(rec, "NAXIS", 5) == 0 || strncmp(rec, "BITPIX", 6) == 0) // NAXIS, NAXISxxx, BITPIX
				continue;
			FITSFUN(fits_write_record, fp, rec);
		}
	}
	FITSFUN(fits_write_record, fp, "COMMENT  modified by simple test routine");

	TRYFITS(fits_write_img, fp, TDOUBLE, 1, sz, fits->data);
	TRYFITS(fits_close_file, fp);
	return TRUE;
}
