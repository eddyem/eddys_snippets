/*
 * slalib_and_sofa.c - calculate apparent place by slalib & libsofa
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


#define _GNU_SOURCE 1111  // strcasecmp

#include "sofa.h"
#include <stdio.h>
#include <stdbool.h>
#include <time.h>

#define DBG(...) printf(__VA_ARGS__)

extern void sla_caldj(int*, int*, int*, double*, int*);
extern void sla_amp(double*, double*, double*, double*, double*, double*);
extern void sla_map(double*, double*, double*, double*, double*,double*, double*, double*, double*, double*);
void slacaldj(int y, int m, int d, double *djm, int *j){
	int iy = y, im = m, id = d;
	sla_caldj(&iy, &im, &id, djm, j);
}
void slaamp(double ra, double da, double date, double eq, double *rm, double *dm ){
	double r = ra, d = da, mjd = date, equi = eq;
	sla_amp(&r, &d, &mjd, &equi, rm, dm);
}
// rm,dm - mean RA,Dec (rad), pr,pd - RA,Dec changes per Julian year (dRA/dt, dDec/dt)
// px - parallax (arcsec), rv - radial speed (km/sec,  +ve if receding)
// eq - epoch and equinox of star data (Julian)
// date - TDB for apparent place (JD-2400000.5)
void slamap(double rm, double dm, double pr, double pd,
             double px, double rv, double eq, double date,
             double *ra, double *da){
	double r = rm, d = dm, p1 = pr, p2 = pd, ppx = px, prv = rv, equi = eq, dd = date;
	sla_map(&r, &d, &p1, &p2, &ppx, &prv, &equi, &dd, ra, da);
}

void reprd(char* s, double ra, double dc){
	char pm;
	int i[4];
	printf ( "%30s", s );
	iauA2tf ( 7, ra, &pm, i );
	printf ( " %2.2d %2.2d %2.2d.%7.7d", i[0],i[1],i[2],i[3] );
	iauA2af ( 6, dc, &pm, i );
	printf ( " %c%2.2d %2.2d %2.2d.%6.6d\n", pm, i[0],i[1],i[2],i[3] );
}

void radtodeg(double r){
	int i[4]; char pm;
	int rem = (int)(r / D2PI);
	if(rem) r -= D2PI * rem;
	if(r > DPI) r -= D2PI;
	else if(r < -DPI) r += D2PI;
	iauA2af (2, r, &pm, i);
	printf("%c%02d %02d %02d.%2.d", pm, i[0],i[1],i[2],i[3]);
}

double getta(char *str){
	int a,b,s = 1; double c;
	if(3 != sscanf(str, "%d:%d:%lf", &a,&b,&c)) return -1;
	if(a < 0){ s = -1; a = -a;}
	c /= 3600.;
	c += a + b/60.;
	c *= s;
	return c;
}

int main (int argc, char **argv){
	double rc, dc;
	if(argc == 3){
		rc = getta(argv[1]) * DPI / 12;
		dc = getta(argv[2]) * DD2R;
	}else{
		/* Star ICRS RA,Dec (radians). */
		if ( iauTf2a ( ' ', 19, 50, 47.6, &rc ) ) return -1;
		if ( iauAf2a ( '+', 8, 52, 12.3, &dc ) ) return -1;
	}
	reprd ( "ICRS, epoch J2000.0:", rc, dc );

	struct tm tms;
	time_t t = time(NULL);
	gmtime_r(&t, &tms);
	int y, m, d, err;
	y = 1900 + tms.tm_year;
	m = tms.tm_mon + 1;
	d = tms.tm_mday;
	double mjd, add = ((double)tms.tm_hour + (double)tms.tm_min/60.0 + tms.tm_sec/3600.0) / 24.;
	DBG("Date: (d/m/y +frac)  %d/%d/%d +%g\n", d, m, y, add);
	slacaldj(y, m, d, &mjd, &err);
	if(err){
		fprintf(stderr, "slacaldj(): Wrong %s!", (err == 1) ? "year" :
			(err == 2? "month" : "day"));
		return -1;
	}
	mjd += add;
	DBG("MJD by slalib: %g\n", mjd);
	double utc1, utc2;
	/* UTC date. */
	if(iauDtf2d("UTC", y, m, d, tms.tm_hour, tms.tm_min, tms.tm_sec,
		&utc1, &utc2)) return -1;
	DBG("UTC by sofa: %g, %g\n", utc1 - 2400000.5, utc2);
	double tai1, tai2, tt1, tt2;
	/* TT date. */
	if ( iauUtctai ( utc1, utc2, &tai1, &tai2 ) ) return -1;
	if ( iauTaitt ( tai1, tai2, &tt1, &tt2 ) ) return -1;
	DBG("date by sofa (utc/tt): %g/%g & %g/%g\n", tai1 - 2400000.5, tt1 - 2400000.5, tai2, tt2);

	double pmra=0, pr=0, pd=0, px=0, rv=0;
	/*
	// Proper motion: RA/Dec derivatives, epoch J2000.0.
	pmra = 536.23e-3 * DAS2R;
	pr = atan2 ( pmra, cos(dc) );
	pd = 385.29e-3 * DAS2R;
	// Parallax (arcsec) and recession speed (km/s).
	px = 0.19495;
	rv = -26.1;*/
	double ri, di, eo;
	/* ICRS to CIRS (geocentric observer). */
	iauAtci13 ( rc, dc, pr, pd, px, rv, tt1, tt2, &ri, &di, &eo );
	reprd ( "catalog -> CIRS:", ri, di );
	double rca, dca;
	/* CIRS to ICRS (astrometric). */
	iauAtic13 ( ri, di, tt1, tt2, &rca, &dca, &eo );
	reprd ( "CIRS -> astrometric:", rca, dca );
	/* ICRS (astrometric) to CIRS (geocentric observer). */
	iauAtci13 ( rca, dca, 0.0, 0.0, 0.0, 0.0, tt1, tt2, &ri, &di, &eo );
	reprd ( "astrometric -> CIRS:", ri, di );
	double ra, da;
	/* Apparent place. */
	ra = iauAnp ( ri - eo );
	da = di;
	reprd ( "geocentric apparent:", ra, da );
	slamap(rc, dc, pmra, pd, px, rv, 2000., mjd, &ra, &da);
	reprd ( "geocentric apparent (sla):", ra, da );
	double ra2000, decl2000;
	slaamp(ra, da, mjd, 2000.0, &ra2000, &decl2000);
	reprd ( "apparent -> astrometric (sla):", ra2000, decl2000);

	double elong, phi, hm, phpa, tc, rh, wl, xp, yp, dut1;
	/* Site longitude, latitude (radians) and height above the geoid (m). */
	iauAf2a ( '+', 41, 26, 26.45, &elong );
	iauAf2a ( '+', 43, 39, 12.69, &phi );
	hm = 2070.0;
	/* Ambient pressure (HPa), temperature (C) and rel. humidity (frac). */
	phpa = 770.0; // milliBar or hectopascal
	tc = -5.0;
	rh = 0.7;
	/* Effective wavelength (microns) */
	wl = 0.55;
	/* EOPs: polar motion in radians, UT1-UTC in seconds. */
	xp = 0.1074 * DAS2R; //polarX
	yp = 0.2538 * DAS2R;//polarY
	dut1 = 0.13026 ; // DUT1
	/* ICRS to observed. */
	double aob, zob, hob, dob, rob;
	if ( iauAtco13 ( rc, dc, pr,
	 pd, px, rv, utc1, utc2, dut1, elong, phi,
	 hm, xp, yp, phpa, tc, rh, wl, &aob, &zob,
	 &hob, &dob, &rob, &eo ) ) return -1;
	reprd ( "ICRS -> observed:",  rob, dob );
	printf("A(bta)/Z: ");
	radtodeg(aob);
	printf("("); radtodeg(DPI-aob);
	printf(")/"); radtodeg(zob);
	printf("\n");
	if( iauAtoc13 ( "R", rc, dc, utc1, utc2, dut1,
	//if( iauAtoc13 ( "R", rob, dob, 2451545, 0, dut1,
	 elong, phi, hm, xp, yp, phpa, tc, rh, wl, &rca, &dca )) return -1;
	reprd ( "observed -> astrometric:", rca, dca );
	return 0;
}


