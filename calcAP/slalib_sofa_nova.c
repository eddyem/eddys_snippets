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

/**
  SOFA - NOVA - SLA
  comparison
**/

#include <stdio.h>
#include <stdbool.h>
#include <sys/time.h>
#include <time.h>
#include <sofa.h>
#include <erfa.h> //- the same data as SOFA
#include <libnova/libnova.h>

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
// apparent->observed
extern void sla_aop(double*, double*, double*, double*, double*, double*, double*, double*, double*, double*, double*, double*, double*, double*, double*, double*, double*, double*, double*);
/*
*  Given:
*     RAP    d      geocentric apparent right ascension
*     DAP    d      geocentric apparent declination
*     DATE   d      UTC date/time (Modified Julian Date, JD-2400000.5)
*     DUT    d      delta UT:  UT1-UTC (UTC seconds)
*     ELONGM d      mean longitude of the observer (radians, east +ve)
*     PHIM   d      mean geodetic latitude of the observer (radians)
*     HM     d      observer's height above sea level (metres)
*     XP     d      polar motion x-coordinate (radians)
*     YP     d      polar motion y-coordinate (radians)
*     TDK    d      local ambient temperature (K; std=273.15D0)
*     PMB    d      local atmospheric pressure (mb; std=1013.25D0)
*     RH     d      local relative humidity (in the range 0D0-1D0)
*     WL     d      effective wavelength (micron, e.g. 0.55D0)
*     TLR    d      tropospheric lapse rate (K/metre, e.g. 0.0065D0)
*
*  Returned:
*     AOB    d      observed azimuth (radians: N=0,E=90)
*     ZOB    d      observed zenith distance (radians)
*     HOB    d      observed Hour Angle (radians)
*     DOB    d      observed Declination (radians)
*     ROB    d      observed Right Ascension (radians)
*/
void slaaop(double rap, double dap, double date, double dut, double elongm, double phim,
            double hm, double xp, double yp, double tdk, double pmb, double rh, double wl,
            double tlr,
            double* aob, double* zob, double* hob, double* dob, double* rob){
    double _rap=rap, _dap=dap, _date=date, _dut=dut, _elongm=elongm, _phim=phim,
            _hm=hm, _xp=xp, _yp=yp, _tdk=tdk, _pmb=pmb, _rh=rh, _wl=wl, _tlr=tlr;
    sla_aop(&_rap, &_dap, &_date, &_dut, &_elongm, &_phim, &_hm, &_xp, &_yp, &_tdk,
            &_pmb, &_rh, &_wl, &_tlr, aob, zob, hob, dob, rob);
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
    reprd ( "ICRS (catalog), epoch J2000.0:", rc, dc );

    struct tm tms;
    struct timeval currentTime;
    gettimeofday(&currentTime, NULL);
    gmtime_r(&currentTime.tv_sec, &tms);
    double tSeconds = tms.tm_sec + ((double)currentTime.tv_usec)/1e6;
    int y, m, d, err;
    y = 1900 + tms.tm_year;
    m = tms.tm_mon + 1;
    d = tms.tm_mday;
    double mjd, add = ((double)tms.tm_hour + (double)tms.tm_min/60.0 + tSeconds/3600.0) / 24.;
    DBG("Date: (d/m/y +frac)  %d/%d/%d +%.8f\n", d, m, y, add);
    slacaldj(y, m, d, &mjd, &err);
    if(err){
        fprintf(stderr, "slacaldj(): Wrong %s!", (err == 1) ? "year" :
            (err == 2? "month" : "day"));
        return -1;
    }
    mjd += add;
    DBG("MJD by slalib: %.8f\n", mjd);
    double utc1, utc2;
    /* UTC date. */
    if(iauDtf2d("UTC", y, m, d, tms.tm_hour, tms.tm_min, tSeconds,
        &utc1, &utc2)) return -1;
    DBG("UTC by sofa: %g, %.8f\n", utc1 - 2400000.5, utc2);
    double tai1, tai2, tt1, tt2;
    /* TT date. */
    if ( iauUtctai ( utc1, utc2, &tai1, &tai2 ) ) return -1;
    if ( iauTaitt ( tai1, tai2, &tt1, &tt2 ) ) return -1;
    DBG("date by sofa (TAI/TT): %g/%g & %g/%g\n", tai1 - 2400000.5, tt1 - 2400000.5, tai2, tt2);

    double pmra=0;
    double pr = 0.0;     // RA proper motion (radians/year; Note 2)
    double pd = 0.0;     // Dec proper motion (radians/year)
    double px = 0.0;     // parallax (arcsec)
    double rv = 0.0;     // radial velocity (km/s, positive if receding)
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
    reprd ( "ICRS -> CIRS:", ri, di );
    double rca, dca, eo1;
    /* CIRS to ICRS (astrometric). */
    iauAtic13 ( ri, di, tt1, tt2, &rca, &dca, &eo1);
    reprd ( "CIRS -> ICRSc:", rca, dca );
    /* ICRS to CIRS without PM
    iauAtci13 ( rca, dca, 0.0, 0.0, 0.0, 0.0, tt1, tt2, &ri, &di, &eo );
    reprd ( "ICRSc -> CIRS (without PM):", ri, di ); */
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
    phpa = 780.0; tc = -5.0; rh = 0.7;
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

    /*
    *  Given:
    *     RAP    d      geocentric apparent right ascension
    *     DAP    d      geocentric apparent declination
    *     DATE   d      UTC date/time (Modified Julian Date, JD-2400000.5)
    *     DUT    d      delta UT:  UT1-UTC (UTC seconds)
    *     ELONGM d      mean longitude of the observer (radians, east +ve)
    *     PHIM   d      mean geodetic latitude of the observer (radians)
    *     HM     d      observer's height above sea level (metres)
    *     XP     d      polar motion x-coordinate (radians)
    *     YP     d      polar motion y-coordinate (radians)
    *     TDK    d      local ambient temperature (K; std=273.15D0)
    *     PMB    d      local atmospheric pressure (mb; std=1013.25D0)
    *     RH     d      local relative humidity (in the range 0D0-1D0)
    *     WL     d      effective wavelength (micron, e.g. 0.55D0)
    *     TLR    d      tropospheric lapse rate (K/metre, e.g. 0.0065D0)
    *
    *  Returned:
    *     AOB    d      observed azimuth (radians: N=0,E=90)
    *     ZOB    d      observed zenith distance (radians)
    *     HOB    d      observed Hour Angle (radians)
    *     DOB    d      observed Declination (radians)
    *     ROB    d      observed Right Ascension (radians)
    */
    slaaop(ra, da, mjd, dut1, elong, phi, hm, xp, yp, tc+273., phpa, rh, wl, 0.0065,
           &aob, &zob, &hob, &dob, &rob);
    reprd ( "ICRS -> observed (sla):",  rob, dob );
    printf("A(bta)/Z: ");
    radtodeg(aob);
    printf("("); radtodeg(DPI-aob);
    printf(")/"); radtodeg(zob);
    printf("\n");

    if( iauAtoc13 ( "R", rob, dob, utc1, utc2, dut1,
     elong, phi, hm, xp, yp, phpa, tc, rh, wl, &rca, &dca )) return -1;
    reprd ( "observed -> ICRS:", rca, dca );

    ////////////////////////////////////////////////////////////////////////////////////////////////
    // libNOVA
    ////////////////////////////////////////////////////////////////////////////////////////////////
    struct ln_equ_posn mean_position;
    mean_position.ra  = rc * ERFA_DR2D; // radians to degrees
    mean_position.dec = dc * ERFA_DR2D;
/*
    struct timeval tv;
    struct timezone tz;
    gettimeofday(&tv, &tz); // number of seconds since the Epoch, 1970-01-01 00:00:00 +0000 (UTC) with microsecond precision
    struct tm *utc_tm;
    utc_tm = gmtime(&tv.tv_sec);
    struct ln_date date;
    date.seconds = utc_tm->tm_sec + ((double)tv.tv_usec / 1000000);
    date.minutes = utc_tm->tm_min;
    date.hours   = utc_tm->tm_hour;
    date.days    = utc_tm->tm_mday;
    date.months  = utc_tm->tm_mon + 1;
    date.years   = utc_tm->tm_year + 1900;
    double JNow = ln_get_julian_day(&date);*/
    double JNow = ln_get_julian_from_sys();
    struct ln_equ_posn propm={0,0}, apppl;//, equprec;
    ln_get_apparent_posn(&mean_position, &propm, JNow, &apppl);
    reprd ("geocentric apparent (NOVA):",  apppl.ra*ERFA_DD2R, apppl.dec*ERFA_DD2R);
    // Calculate the effects of precession on equatorial coordinates, between arbitary Jxxxx epochs.
/*
    ln_get_equ_prec2(&mean_position, JD2000, JNow, &equprec);
    reprd ("ln_get_equ_prec2 (NOVA):",  equprec.ra*ERFA_DD2R, equprec.dec*ERFA_DD2R);
    */
/*
    // ERFA
    struct tm *ts;
    ts = gmtime(&t);
    int result = eraDtf2d ( "UTC", ts->tm_year+1900, ts->tm_mon+1, ts->tm_mday, ts->tm_hour, ts->tm_min, ts->tm_sec, &utc1, &utc2 );
    if (result != 0) {
        printf("eraDtf2d call failed\n");
        return 1;
    }
    // Make TT julian date for Atci13 call
    result = eraUtctai( utc1, utc2, &tai1, &tai2 );
    if (result != 0) {
        printf("eraUtctai call failed\n");
        return 1;
    }
    eraTaitt(  tai1, tai2, &tt1,  &tt2  );
    eraAtci13 ( rc, dc, pr, pd, px, rv, tt1, tt2, &ri, &di, &eo );
    reprd ( "geocentric apparent (ERFA):",  eraAnp(ri - eo), di);
*/
    return 0;
}

