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

// Messages for blocking: GSV, RMC, GSA, GGA, GLL, VTG
char *GPmsgs[] = {"GSV", "RMC", "GSA", "GGA", "GLL", "VTG"};

static void signals(int sig){
	DBG("Get signal %d, quit.\n", sig);
	restore_tty();
	exit(sig);
}

uint8_t *get_portdata(){
	static uint8_t buf[1025];
	uint8_t *ptr = buf;
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
 * Calculate checksum & write message to port
 * @param  buf - command to write (with leading $ and trailing *)
 * return 0 if fails
 */
int write_with_checksum(uint8_t *buf){
	static char CS[3];
	//uint8_t *ptr = buf;
	uint8_t checksum = 0;
	if(*buf != '$') return 0;
	if(write_tty(buf, strlen((char*)buf))) return 0;
	++buf; // skip leaders
	do{
		checksum ^= *buf++;
	}while(*buf && *buf != '*');
	snprintf(CS, 3, "%X", checksum);
	if(write_tty((uint8_t*)CS, 2)) return 0;
	if(write_tty((uint8_t*)"\r\n", 2)) return 0;
	//DBG("Write: %s%c%c", ptr, CS[0], CS[1]);
	return 1;
}

// Check checksum
int checksum(uint8_t *buf){
	uint8_t *eol;
	char chs[3];
	uint8_t checksum = 0;
	if(*buf != '$' || !(eol = (uint8_t*)strchr((char*)buf, '*'))){
		DBG("Wrong data: %s\n", buf);
		return 0;
	}
	while(++buf != eol)
		checksum ^= *buf;
	snprintf(chs, 3, "%X", checksum);
	if(strncmp(chs, (char*)++buf, 2)) return 0;
	return 1;
}

uint8_t *nextpos(uint8_t **buf, int pos){
	int i;
	if(pos < 1) pos = 1;
	for(i = 0; i < pos; ++i){
		*buf = (uint8_t*)strchr((char*)*buf, ',');
		if(!*buf) break;
		++(*buf);
	}
	return *buf;
}
#define NEXT()      do{if(!nextpos(&buf, 1)) goto ret;}while(0)
#define SKIP(NPOS)  do{if(!nextpos(&buf, NPOS)) goto ret;}while(0)

/*
 * Satellites in View
 * $GPGSV,NoMsg,MsgNo,NoSv,{,sv,elv,az,cno}*cs
 * 1    = total number of GPGSV messages being output
 * 2    = Number of this message
 * 3    = Satellites in View
 *    4-7 will be repeated 1..4 times
 * sv   = Satellite ID
 * elv  = Elevation, range 0..90deg
 * az   = Azimuth, range 0..359deg
 * cno  = SNR, range 0.99dB
 * cs   = control sum
 * 3,1,11,01,68,278,,03,39,292,08,04,66,190,30,11,55,231,34*79
 * 3,2,11,14,30,050,10,17,11,322,,19,09,202,,22,14,096,*74
 * 3,3,11,23,20,232,30,31,43,118,33,32,76,352,*43
*/
void gsv(uint8_t *buf){
	//DBG("gsv: %s\n", buf);
	int Nrec = -1, Ncur, inview = 0;
	static int sat_scanned = 0;
	if(sscanf((char*)buf, "%d,%d,", &Nrec, &Ncur) != 2) goto ret;
	SKIP(2);
	if(sscanf((char*)buf, "%d,", &inview) != 1) goto ret;
	if(inview < 1) goto ret;
	NEXT();
	if(Ncur == 1)
		printf("%d satellites in view field: (No: ID, elevation, azimuth, SNR)\n", inview);
	do{
		int id, el, az, snr;
		// there could be no "SNR" field if we can't find this satellite on sky
		if(sscanf((char*)buf, "%d,%d,%d,%d,", &id, &el, &az, &snr) < 3) break;
		printf(" (%d: %d, %d, %d, %d)", ++sat_scanned, id, el, az, snr);
		SKIP(4);
	}while(1);
ret:
	if(inview < 1) printf("There's no GPS satellites in viewfield\n");
	if(Nrec > 0 && Nrec == Ncur){
		sat_scanned = 0;
		printf("\n");
	}
}

/**
 * acquire last command
 * @return 0 if failed
 */
int get_ACK(uint8_t *conf) {
	int i;
	uint8_t ack[10] = {0xb5, 0x62, // header
		0x05, 0x01, // ACK-ACK
		2, 0}; // 2 bytes, little-endian
	// send ACK to recent CONF changes
	ack[6] = conf[2];
	ack[7] = conf[3];
	double T0 = dtime();
	// checksum
	for (i = 2; i < 8; ++i){
		ack[8] += ack[i];
		ack[9] += ack[8];
	}
/*
printf("ack: ");
for(i = 0; i < 10; ++i)
printf("%X ", ack[i]);
printf("\n");
*/
	uint8_t buf[10], *ptr = buf, *cmp = ack;
	size_t remain = 10;
	// wait not more than 3 seconds
	while(dtime() - T0 < 3. && remain){
		size_t L = read_tty(ptr, remain);
		if(L){
			remain -= L;
			for(i = 0; i < (int)L; ++i){
				//DBG("got: %X (%c)", *ptr, *ptr);
				if(*ptr++ != *cmp++){
					//DBG("NEQ: %X != %X", ptr[-1], cmp[-1]);
					ptr = buf; cmp = ack; remain = 10;
					break;
				}
			}
		}
	}
	if(!remain) return 1;
	return 0;
}

void cfg_stationary(){
	int i;
	uint8_t stat[44] = {0xb5, 0x62, // header
		0x06, 0x24, // CFG-NAV5
		36, 0, // 36 bytes, little-endian
		1, 0,  // mask: only dynamic model
		2};    // stationary model
//	stat[44] = '\r';
//	stat[45] = '\n';
/*
	uint8_t stat[] = {
	0xB5, 0x62, 0x06, 0x24, 0x24, 0x00, 0xFF, 0xFF, 0x02, 0x03,
	0x00, 0x00, 0x00, 0x00, 0x10, 0x27, 0x00, 0x00, 0x05, 0x00,
	0xFA, 0x00, 0xFA, 0x00, 0x64, 0x00, 0x2C, 0x01, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, '\r', '\n'};
	*/
	for(i = 2; i < 42; ++i){
		stat[42] += stat[i];
		stat[43] += stat[42];
	}
/*
printf("conf: ");
for(i = 0; i < 44; ++i)
printf("%X ", conf[i]);
printf("\n");
*/
	i = 0;
	do{
		write_tty(stat, 44);
		DBG("Written, aquire");
	}while(!get_ACK(stat) && ++i < 11);
	// precise point position
	uint8_t prec[48] = {0xb5, 0x62, // header
		0x06, 0x23, // CFG-NAVX5
		40, 0, // 40 bytes, little-endian
		0, 0, 0x20}; // mask for PPP
	prec[32] = 1; // usePPP = TRUE (field 26)
	for(i = 2; i < 46; ++i){
		prec[46] += prec[i];
		prec[47] += prec[46];
	}
	i = 0;
	do{
		write_tty(prec, 48);
		DBG("Written, aquire");
	}while(!get_ACK(prec) && ++i < 11);
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
void rmc(uint8_t *buf){
	//DBG("rmc: %s\n", buf);
	int H, M, LO, LA, d, m, y;
	float S;
	float longitude, lattitude, speed, track, mag;
	char varn = 'V', north = '0', east = '0', mageast = '0';
	sscanf((char*)buf, "%2d%2d%f", &H, &M, &S);
	NEXT();
	if(*buf != ',') varn = *buf;
	if(varn != 'A')
		printf("(data could be wrong)");
	else
		printf("(data valid)");
	printf(" time: %02d:%02d:%05.2f", H, M, S);
	NEXT();
	sscanf((char*)buf, "%2d%f", &LA, &lattitude);
	NEXT();
	if(*buf != ','){
		north = *buf;
		lattitude = (float)LA + lattitude / 60.;
		if(north == 'S') lattitude = -lattitude;
		printf(" latt: %f", lattitude);
	}
	NEXT();
	sscanf((char*)buf, "%3d%f", &LO, &longitude);
	NEXT();
	if(*buf != ','){
		east = *buf;
		longitude = (float)LO + longitude / 60.;
		if(east == 'W') longitude = -longitude;
		printf(" long: %f", longitude);
	}
	NEXT();
	if(*buf != ','){
		sscanf((char*)buf, "%f", &speed);
		printf(" speed: %fknots", speed);
	}
	NEXT();
	if(*buf != ','){
		sscanf((char*)buf, "%f", &track);
		printf(" track: %fdeg,True", track);
	}
	NEXT();
	if(sscanf((char*)buf, "%2d%2d%2d", &d, &m, &y) == 3)
		printf(" date(dd/mm/yy): %02d/%02d/%02d", d, m, y);
	NEXT();
	sscanf((char*)buf, "%f,%c*", &mag, &mageast);
	if(mageast == 'E' || mageast == 'W'){
		if(mageast == 'W') mag = -mag;
		printf(" magnetic var: %f", mag);
	}
ret:
	printf("\n");
}

/*
 * Overall Satellite data
 * $GPGSA,Smode,FS{,sv},PDOP,HDOP,VDOP*cs
 * 1     = mode: 'M' - manual, 'A' - auto
 * 2     = fix status: 1 - not available, 2 - 2D, 3 - 3D
 * 3..14 = used satellite number
 * 15    = position dilution
 * 16    = horizontal dilution
 * 17    = vertical dilution
 * A,2,04,31,32,14,19,,,,,,,,2.77,2.58,1.00*05
 */
void gsa(uint8_t *buf){
	//DBG("gsa: %s\n", buf);
	int used[12];
	int i, Nused = 0;
	if(*buf == 'M') printf("Mode: manual; ");
	else if(*buf == 'A') printf("Mode: auto; ");
	else return; // wrong data
	NEXT();
	switch(*buf){
		case '1':
			printf("no fix; ");
		break;
		case '2':
			printf("2D fix; ");
		break;
		case '3':
			printf("3D fix; ");
		break;
		default:
			goto ret;
	}
	NEXT();
	for(i = 0; i < 12; ++i){
		int N;
		if(sscanf((char*)buf, "%d,", &N) == 1)
			used[Nused++] = N;
		NEXT();
	}
	if(Nused){
		printf("%d satellites used: ", Nused);
		for(i = 0; i < Nused; ++i)
			printf("%d, ", used[i]);
	}
	float pos, hor, vert;
	if(sscanf((char*)buf, "%f,%f,%f", &pos, &hor, &vert) == 3){
		printf("DILUTION: pos=%.2f, hor=%.2f, vert=%.2f", pos, hor, vert);
	}
ret:
	printf("\n");
}

// 213457.00,4340.59415,N,04127.47560,E,1,05,2.58,1275.8,M,17.0,M,,*60
void gga(_U_ uint8_t *buf){
	//DBG("gga: %s\n", buf);
}

//4340.59415,N,04127.47560,E,213457.00,A,A*60
void gll(_U_ uint8_t *buf){
	//DBG("gll: %s\n", buf);
}

// ,T,,M,2.494,N,4.618,K,A*23
void vtg(_U_ uint8_t *buf){
	//DBG("vtg: %s\n", buf);
}


void txt(_U_ uint8_t *buf){
	DBG("txt: %s\n", buf);
}

/**
 * PUBX,00
 * $PUBX,00,hhmmss.ss,Latitude,N,Longitude,E,AltRef,NavStat,Hacc,Vacc,SOG,COG,Vvel,ageC,HDOP,VDOP,TDOP,GU,RU,DR,*cs
 * here buf starts from hhmmss.ss == 1
 * 1  = UTC time
 * 2  = lattitude
 * 3  = N/S
 * 4  = longitude
 * 5  = E/W
 * 6  = altitude
 * 7  = nav.stat: NF/DR/G2/G3/D2/D3/RK/TT
 * 8  = horizontal accuracy
 * 9  = vertical accuracy
 * 10 = speed over ground (km/h)
 * 11 = cource over ground (deg)
 * 12 = vertical velocy ("+" -- down, "-" -- up)
 * 13 = age of most recent DGPS correction
 * 14 = hor. dilution
 * 15 = vert. dilution
 * 16 = time dilution
 * 17 = number of GPS satell. used
 * 18 = number of GLONASS sat. used
 * 19 = DR used
 * $PUBX,00,113123.00,4340.61823,N,04127.45581,E,1295.919,G2,30,6.9,1.875,38.17,0.000,,2.77,1.00,1.41,3,0,0*4C
 */
void pubx(uint8_t *buf){
	//DBG("pubx_00: %s\n", buf);
	int H, M, LO, LA, gps, glo, dr;
	float S, longitude, lattitude, altitude, hacc, vacc, speed, track, vertspd,
		age, hdop, vdop, tdop;
	char north = '0', east = '0';
	sscanf((char*)buf, "%2d%2d%f", &H, &M, &S);
	printf("time: %02d:%02d:%05.2f", H, M, S);
	NEXT();
	sscanf((char*)buf, "%2d%f", &LA, &lattitude);
	NEXT();
	if(*buf != ','){
		north = *buf;
		lattitude = (float)LA + lattitude / 60.;
		if(north == 'S') lattitude = -lattitude;
		printf(" latt: %f", lattitude);
	}
	NEXT();
	sscanf((char*)buf, "%3d%f", &LO, &longitude);
	NEXT();
	if(*buf != ','){
		east = *buf;
		longitude = (float)LO + longitude / 60.;
		if(east == 'W') longitude = -longitude;
		printf(" long: %f", longitude);
	}
	NEXT();
#define FSCAN(par, nam) do{if(*buf != ','){sscanf((char*)buf, "%f", &par); \
		printf(" " nam ": %f", par);} NEXT();}while(0)
	FSCAN(altitude, "altitude");
	if(*buf != ','){
		printf(" nav. status: %c%c", buf[0], buf[1]);
	}
	NEXT();
	FSCAN(hacc,"hor.accuracy");
	FSCAN(vacc, "vert.accuracy");
	FSCAN(speed,"speed");
	FSCAN(track,"cource");
	FSCAN(vertspd,"vertical speed ('+'-down)");
	FSCAN(age,"DGPS age");
	FSCAN(hdop, "hor. dilution");
	FSCAN(vdop, "vert. dilution");
	FSCAN(tdop, "time dilution");
#undef FSCAN
#define ISCAN(par, nam) do{if(*buf != ','){sscanf((char*)buf, "%d", &par); \
		printf(" " nam ": %d", par);} NEXT();}while(0)
	ISCAN(gps, "GPS used");
	ISCAN(glo, "GLONASS used");
	ISCAN(dr, "DR used");
#undef ISCAN
ret:
	printf("\n");
}

/**
 * Parce content of buffer with GPS data
 * WARNING! This function changes data content
 */
void parce_data(uint8_t *buf){
	uint8_t *eol;
	//DBG("GOT: %s", buf);
	while(*buf && (eol = (uint8_t*)strchr((char*)buf, '\r'))){
		*eol = 0;
		// now make checksum checking:
		if(!checksum(buf)) goto cont;
		if(strncmp((char*)buf, "$GP", 3)){
			if(strncmp((char*)buf, "$PUBX,00,", 9) == 0){
				buf += 9;
				pubx(buf);
			}else{
				DBG("Bad string: %s\n", buf);
			}
			goto cont;
		}
		buf += 3;
		// PARSE variants: GSV, RMC, GSA, GGA, GLL, VTG, TXT
		// 1st letter, cold be one of G,R,V or T
		switch(*buf){
			case 'G': // GSV, GSA, GGA, GLL
				++buf;
				if(strncmp((char*)buf, "SV", 2) == 0){
					gsv(buf+3);
				}else if(strncmp((char*)buf, "SA", 2) == 0){
					gsa(buf+3);
				}else if(strncmp((char*)buf, "GA", 2) == 0){
					gga(buf+3);
				}else if(strncmp((char*)buf, "LL", 2) == 0){
					gll(buf+3);
				}else{
					DBG("Unknown: $GPG%s", buf);
					goto cont;
				}
			break;
			case 'R': // RMC
				++buf;
				if(strncmp((char*)buf, "MC", 2) == 0){
					rmc(buf+3);
				}else{
					DBG("Unknown: $GPR%s", buf);
					goto cont;
				}
			break;
			case 'V': // VTG
				++buf;
				if(strncmp((char*)buf, "TG", 2) == 0){
					vtg(buf+3);
				}else{
					DBG("Unknown: $GPV%s", buf);
					goto cont;
				}
			break;
			case 'T': // TXT
				++buf;
				if(strncmp((char*)buf, "XT", 2) == 0){
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
	cont:
		if(eol[1] != '\n') break;
		buf = eol + 2;
	}
}

/**
 * set rate for given NMEA field
 * @param field - name of NMEA field
 * @param rate  - rate in seconds (0 disables field)
 * @return -1 if fails, rate if OK
 */
int nmea_fieldrate(uint8_t *field, int rate){
	uint8_t buf[256];
	if(rate < 0) return -1;
	snprintf((char*)buf, 255, "$PUBX,40,%s,0,%d,0,0*", field, rate);
	if(write_with_checksum(buf)) return rate;
	else return -1;
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
	int i;
	for(i = 0; i < GPMAXMSG; ++i){
		int block = 0;
		if(GP->block_msg[i])
			block = 1; // unblock message
		if(nmea_fieldrate((uint8_t*)GPmsgs[i], block) != block)
			WARN("Can't %sblock %s!", block?"un":"", GPmsgs[i]);
		else
			printf("%sblock %s\n", block?"un":"", GPmsgs[i]);
	}
	if(GP->stationary){
		printf("stationary config\n");
		cfg_stationary();
	}
	double T, T0 = dtime(), Tpoll = 0., tmout = GP->polltmout;
	uint8_t *str = NULL;
	while((T=dtime()) - T0 < tmout){
		if(GP->pollubx && T-Tpoll > GP->pollinterval){
			Tpoll = T;
			write_tty((uint8_t*)"$PUBX,00*33\r\n", 13);
		}
		if((str = get_portdata())){
			parce_data(str);
		}
	}
	signals(0);
	return 0;
}
