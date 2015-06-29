/*
 * main.c
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

#include "usefull_macros.h"
#include "cmdlnopts.h"
#include "daemon.h"

#include <assert.h>
#include <signal.h>

#ifndef PIDFILE
#define PIDFILE  "/tmp/GPStest.pid"
#endif

glob_pars *GP = NULL;

static void signals(int sig){
	DBG("Get signal %d, quit.\n", sig);
	restore_tty();
	exit(sig);
}

char *get_portdata(){
	static char buf[1025];
	char *ptr = buf;
	size_t L = 0, rest = 1024;
	while(rest && (L = read_tty(ptr, rest))){
		rest -= L;
		ptr += L;
	}
	if(ptr != buf){
		*ptr = 0;
		ptr = buf;
	}else ptr = NULL;
	return ptr;
}

/**
 * Checksum: return 0 if fails
 */
int checksum(char *buf){
	char *eol;
	char chs[2];
	unsigned char checksum = 0;
	if(*buf != '$' || !(eol = strchr(buf, '*'))){
		DBG("Wrong data: %s\n", buf);
		return 0;
	}
	while(++buf != eol)
		checksum ^= (unsigned char) (*buf);
	snprintf(chs, 2, "%X", checksum);
	if(!strncmp(chs, ++buf, 2)) return 0;
	return 1;
}

void gsv(_U_ char *buf){
	//DBG("gsv: %s\n", buf);
}

/*
 * Recommended minimum specific GPS/Transit data
 * $GPRMC,hhmmss.ss,A,llll.ll,a,yyyyy.yy,a,x.x,x.x,ddmmyy,x.x,a*hh
 * 1    = UTC of position fix
 * 2    = Data status (V=navigation receiver warning)
 * 3    = Latitude of fix
 * 4    = N or S
 * 5    = Longitude of fix
 * 6    = E or W
 * 7    = Speed over ground in knots
 * 8    = Track made good in degrees True
 * 9    = UT date
 * 10   = Magnetic variation degrees (Easterly var. subtracts from true course)
 * 11   = E or W
 * 12   = Checksum
 * 213457.00,A,4340.59415,N,04127.47560,E,2.494,,290615,,,A*7B
 */
void rmc(char *buf){
	int H, M, LO, LA, d, m, y;
	float S;
	float longitude, lattitude, speed, track, mag;
	char varn = 'V', north = '0', east = '0', mageast = '0';
	sscanf(buf, "%2d%2d%f", &H, &M, &S);
	buf = strchr(buf, ',')+1;
	if(*buf != ',') varn = *buf;
	if(varn != 'A')
		printf("(data could be wrong)");
	else
		printf("(data valid)");
	printf(" time: %02d:%02d:%05.2f", H, M, S);
	buf = strchr(buf, ',')+1;
	sscanf(buf, "%2d%f", &LA, &lattitude);
	buf = strchr(buf, ',')+1;
	if(*buf != ','){
		north = *buf;
		lattitude = (float)LA + lattitude / 60.;
		if(north == 'S') lattitude = -lattitude;
		printf(" latt: %f", lattitude);
	}
	buf = strchr(buf, ',')+1;
	sscanf(buf, "%3d%f", &LO, &longitude);
	buf = strchr(buf, ',')+1;
	if(*buf != ','){
		east = *buf;
		longitude = (float)LO + longitude / 60.;
		if(east == 'W') longitude = -longitude;
		printf(" long: %f", longitude);
	}
	buf = strchr(buf, ',')+1;
	if(*buf != ','){
		sscanf(buf, "%f", &speed);
		printf(" speed: %fknots", speed);
	}
	buf = strchr(buf, ',')+1;
	if(*buf != ','){
		sscanf(buf, "%f", &track);
		printf(" track: %fdeg,True", track);
	}
	buf = strchr(buf, ',')+1;
	if(sscanf(buf, "%2d%2d%2d", &d, &m, &y) == 3)
		printf(" date(dd/mm/yy): %02d/%02d/%02d", d, m, y);
	buf = strchr(buf, ',')+1;
	sscanf(buf, "%f,%c*", &mag, &mageast);
	if(mageast == 'E' || mageast == 'W'){
		if(mageast == 'W') mag = -mag;
		printf(" magnetic var: %f", mag);
	}
	printf("\n");
}

// A,2,04,31,32,14,19,,,,,,,,2.77,2.58,1.00*05
void gsa(_U_ char *buf){
	//DBG("gsa: %s\n", buf);
}

// 213457.00,4340.59415,N,04127.47560,E,1,05,2.58,1275.8,M,17.0,M,,*60
void gga(_U_ char *buf){
	//DBG("gga: %s\n", buf);
}

//4340.59415,N,04127.47560,E,213457.00,A,A*60
void gll(_U_ char *buf){
	//DBG("gll: %s\n", buf);
}

// ,T,,M,2.494,N,4.618,K,A*23
void vtg(_U_ char *buf){
	//DBG("vtg: %s\n", buf);
}


void txt(_U_ char *buf){
	//DBG("txt: %s\n", buf);
}

/**
 * Parce content of buffer with GPS data
 * WARNING! This function changes data content
 */
void parce_data(char *buf){
	char *eol;
	while(buf && (eol = strchr(buf, '\r'))){
		*eol = 0;
		// now make checksum checking:
		if(!checksum(buf)) goto cont;
		if(strncmp(buf, "$GP", 3)){
			DBG("Bad string: %s\n", buf);
			goto cont;
		}
		buf += 3;
		// PARSE variants: GSV, RMC, GSA, GGA, GLL, VTG, TXT
		// 1st letter, cold be one of G,R,V or T
		switch(*buf){
			case 'G': // GSV, GSA, GGA, GLL
				++buf;
				if(strncmp(buf, "SV", 2) == 0){
					gsv(buf+3);
				}else if(strncmp(buf, "SA", 2) == 0){
					gsa(buf+3);
				}else if(strncmp(buf, "GA", 2) == 0){
					gga(buf+3);
				}else if(strncmp(buf, "LL", 2) == 0){
					gll(buf+3);
				}else{
					DBG("Unknown: $GPG%s", buf);
					goto cont;
				}
			break;
			case 'R': // RMC
				++buf;
				if(strncmp(buf, "MC", 2) == 0){
					rmc(buf+3);
				}else{
					DBG("Unknown: $GPR%s", buf);
					goto cont;
				}
			break;
			case 'V': // VTG
				++buf;
				if(strncmp(buf, "TG", 2) == 0){
					vtg(buf+3);
				}else{
					DBG("Unknown: $GPV%s", buf);
					goto cont;
				}
			break;
			case 'T': // TXT
				++buf;
				if(strncmp(buf, "XT", 2) == 0){
					txt(buf+3);
				}else{
					DBG("Unknown: $GPT%s", buf);
					goto cont;
				}
			break;
			default:
				DBG("Unknown: $GP%s", buf);
				goto cont;
		}
	//	printf("GOT: %s\n", buf);
	cont:
		if(eol[1] != '\n') break;
		buf = eol + 2;
	}
}

int main(int argc, char **argv){
	check4running(argv, PIDFILE, NULL);
	initial_setup();
	GP = parce_args(argc, argv);
	assert(GP);
	DBG("Device path: %s\n", GP->devpath);
	tty_init(GP->devpath);
	signal(SIGTERM, signals); // kill (-15) - quit
	signal(SIGHUP, signals);  // hup - quit
	signal(SIGINT, signals);  // ctrl+C - quit
	signal(SIGQUIT, signals); // ctrl+\ - quit
	signal(SIGTSTP, SIG_IGN); // ignore ctrl+Z
	double T0 = dtime();
	char *str = NULL;
	while(dtime() - T0 < 10.){
		if((str = get_portdata()))
			parce_data(str);
	}
	signals(0);
	return 0;
}
