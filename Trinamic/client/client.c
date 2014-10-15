/*
 * client.c - simple terminal client
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
#include <termios.h>		// tcsetattr
#include <unistd.h>			// tcsetattr, close, read, write
#include <sys/ioctl.h>		// ioctl
#include <stdio.h>			// printf, getchar, fopen, perror
#include <stdlib.h>			// exit
#include <sys/stat.h>		// read
#include <fcntl.h>			// read
#include <signal.h>			// signal
#include <time.h>			// time
#include <string.h>			// memcpy
#include <stdint.h>			// int types
#include <sys/time.h>		// gettimeofday

#define BUFLEN 1024

double t0; // start time

uint32_t motor_speed = 10; // motor speed

FILE *fout = NULL; // file for messages duplicating
char *comdev = "/dev/ttyUSB1";
int BAUD_RATE = B115200;
struct termio oldtty, tty; // TTY flags
struct termios oldt, newt; // terminal flags
int comfd; // TTY fd

#define DBG(...) do{fprintf(stderr, __VA_ARGS__);}while(0)

/**
 * function for different purposes that need to know time intervals
 * @return double value: time in seconds
 *
double dtime(){
	double t;
	struct timeval tv;
	gettimeofday(&tv, NULL);
	t = tv.tv_sec + ((double)tv.tv_usec)/1e6;
	return t;
}*/

/**
 * Exit & return terminal to old state
 * @param ex_stat - status (return code)
 */
void quit(int ex_stat){
	tcsetattr(STDIN_FILENO, TCSANOW, &oldt); // return terminal to previous state
	ioctl(comfd, TCSANOW, &oldtty ); // return TTY to previous state
	close(comfd);
	if(fout) fclose(fout);
	printf("Exit! (%d)\n", ex_stat);
	exit(ex_stat);
}

int send_command(uint8_t *ninebytes){
	uint8_t crc = 0;
	int i;
	printf("send: ");
	for(i = 0; i < 8; crc += ninebytes[i++])
		printf("%u,", ninebytes[i]);
	ninebytes[8] = crc;
	printf("%u\n",ninebytes[8]);
	if(9 != write(comfd, ninebytes, 9)){
		perror("Can't write to Trinamic");
		return 0;
	}
	return 1;
}


/**
 * Open & setup TTY, terminal
 */
void tty_init(){
	// terminal without echo
	tcgetattr(STDIN_FILENO, &oldt);
	newt = oldt;
	newt.c_lflag &= ~(ICANON | ECHO);
	if(tcsetattr(STDIN_FILENO, TCSANOW, &newt) < 0) quit(-2);
	printf("\nOpen port...\n");
	if ((comfd = open(comdev,O_RDWR|O_NOCTTY|O_NONBLOCK)) < 0){
		fprintf(stderr,"Can't use port %s\n",comdev);
		quit(1);
	}
	printf(" OK\nGet current settings...\n");
	if(ioctl(comfd,TCGETA,&oldtty) < 0) quit(-1); // Get settings
	tty = oldtty;
	tty.c_lflag     = 0; // ~(ICANON | ECHO | ECHOE | ISIG)
	tty.c_oflag     = 0;
	tty.c_cflag     = BAUD_RATE|CS8|CREAD|CLOCAL; // 9.6k, 8N1, RW, ignore line ctrl
	tty.c_cc[VMIN]  = 0;  // non-canonical mode
	tty.c_cc[VTIME] = 5;
	if(ioctl(comfd,TCSETA,&tty) < 0) quit(-1); // set new mode
	printf(" OK\n");
	tcsetattr(STDIN_FILENO, TCSANOW, &newt);
}

/**
 * getchar() without echo
 * wait until at least one character pressed
 * @return character readed
 *
int mygetchar(){
	int ret;
	do ret = read_console();
	while(ret == 0);
	return ret;
}*/

/**
 * read both tty & console
 * @param buff (o)    - buffer for messages readed from tty or NULL (to omit readed info)
 * @param length (io) - buff's length (return readed len or 0)
 * @param rb (io)      - byte readed from console, -1 if nothing read, NULL to omit console reading
 * @return 1 if something was readed here or there
 */
int read_tty_and_console(uint8_t *buff, size_t *length, int *rb){
	ssize_t L;
	ssize_t l;
	uint8_t buff1[BUFLEN+1];
	size_t buffsz;
	struct timeval tv;
	int sel, retval = 0;
	fd_set rfds;
	FD_ZERO(&rfds);
	FD_SET(STDIN_FILENO, &rfds);
	FD_SET(comfd, &rfds);
	tv.tv_sec = 0; tv.tv_usec = 10000;
	sel = select(comfd + 1, &rfds, NULL, NULL, &tv);
	if(!buff){
		buffsz = BUFLEN;
		buff = buff1;
	}else{
		buffsz = *length;
	}
	if(sel > 0){
		if(rb){
			if(FD_ISSET(STDIN_FILENO, &rfds)){
				*rb = getchar();
				retval = 1;
			}else{
				*rb = -1;
			}
		}
		if(FD_ISSET(comfd, &rfds)){
			if((L = read(comfd, buff, buffsz)) < 1){ // disconnect or other troubles
				fprintf(stderr, "USB error or disconnected!\n");
				quit(1);
			}else{
				if(L == 0){ // USB disconnected
					fprintf(stderr, "USB disconnected!\n");
					quit(1);
				}

				// all OK continue reading
				//DBG("readed %zd bytes, try more.. ", L);
				buffsz -= L;
				usleep(10000);
				while(buffsz > 0 && (l = read(comfd, buff+L, buffsz)) > 0){
					L += l;
					buffsz -= l;
					usleep(10000);
				}
				//DBG("full len: %zd\n", L);

				if(length) *length = (size_t) L;
				retval = 1;
			}
		}else{
			if(length) *length = 0;
		}
	}
	return retval;
}

int32_t get_integer(uint8_t *buff){
	int32_t val;
	int ii;
	uint8_t *valptr = (uint8_t*) &val + 3;
	for(ii = 4; ii < 8; ii++)
		*valptr-- = buff[ii];
	return val;
}

void help(){
	printf("Use this commands:\n"
		"h\tShow this help\n"
		"+/-\tIncrease/decrease speed by 10%% or 1\n"
		"M/m\tMove to relative position\n"
		"R/L\tRotate motor 1 right/left\n"
		"r/l\tRotate motor 2 right/left\n"
		"S/s\tStop 1st or 2nd motor\n"
		"D/d\tSet ustepping to 1/16\n"
		"E/e\tSet ustepping to 1/4\n"
		"Z/z\tSet to zero current position\n"
		"G/g\tGet current position\n"
		"A/a\tGet ALL information about\n"
		"W/w\tWait until position reached\n"
		"q\tQuit\n"
	);
}

typedef struct{
	char *name; // command name
	uint8_t cmd;    // command number
} custom_command;

void get_information(uint8_t *com){
	int i;
	uint8_t buff[9];
	size_t L;
	custom_command info[] = {
		{"target position", 0},
		{"actual position", 1},
		{"target speed", 2},
		{"actual speed", 3},
		{"max positioning speed", 4},
		{"max acceleration", 5},
		{"abs. max current", 6},
		{"standby current", 7},
		{"target pos. reached", 8},
		{"ref. switch status", 9},
		{"right limit switch status", 10},
		{"left  limit switch status", 11},
		{"right limit switch disable", 12},
		{"left limit switch disable", 13},
		{"minimum speed", 130},
		{"actual acceleration", 135},
		{"ramp mode", 138},
		{"microstep resolution", 140},
		{"soft stop flag", 149},
		{"ramp divisor", 153},
		{"pulse divisor", 154},
		{"referencing mode", 193},
		{"referencing search speed", 194},
		{"referencing switch speed", 195},
		{"distance end switches", 196},
		{"mixed decay threshold", 203},
		{"freewheeling", 204},
		{"stall detection threshold", 205},
		{"actual load value", 206},
		{"driver error flags", 208},
		{"fullstep threshold", 211},
		{"power down delay", 214},
		{NULL, 0}
	};
	printf("\n");
	for(i = 0; info[i].name; i++){
		printf("%s (%u) ", info[i].name, info[i].cmd);
		com[2] = info[i].cmd;
		if(send_command(com)){ // command send OK
		L = 9;
		if(read_tty_and_console(buff, &L, NULL)){ // get answer
			if(L == 9 && buff[2] > 6){ // answer's length is right && no error in status
				int32_t val = get_integer(buff);
				printf("= %d\n", val);
				//for(ii = 0; ii < 9; ii++) printf("%u,",buff[ii]);
			}else printf("L=%zd (stat: %u)\n", L, buff[2]);
		}}
		printf("\n");
	}
}

void con_sig(int rb){
	if(rb < 1) return;
	uint32_t value = motor_speed; // here is the value to send
	/*
	 * trinamic binary format (bytes):
	 * 0 - address
	 * 1 - command
	 * 2 - type
	 * 3 - motor/bank number
	 * 4 \
	 * 5 | value (msb first!)
	 * 6 |
	 * 7 /
	 * 8 - checksum (sum of bytes 0..8)
	 */
	uint8_t command[9] = {1,0,0,0,0,0,0,0,0}; // command to sent
	int i, V;
	uint8_t *cmd = &command[1]; // byte 1 of message is command itself
	if(rb < 1) return;
	if(rb == 'q' || rb == 'Q') quit(0);
	if(rb == 'h' || rb == 'H'){
		help();
		return;
	}
	void change_motor_speed(int dir){
		uint32_t Delta = motor_speed / 10, newspd;
		int i;
		if(Delta == 0) Delta = 1;
		if(dir > 0){
			newspd = motor_speed + Delta;
			if(newspd > motor_speed) motor_speed = newspd;
			else motor_speed = (uint32_t)0xffffffffUL;
		}else{
			newspd = motor_speed - Delta;
			if(newspd < motor_speed && newspd) motor_speed = newspd;
			else motor_speed = 1;
		}
		printf("\nspeed changed to %u\n", motor_speed);
		*cmd = 5; // SAP
		command[2] = 4; // max pos speed
		uint8_t *spd = (uint8_t*) &motor_speed + 3;
		for(i = 4; i < 8; i++)
			command[i] = *spd--;
		send_command(command); // store new speed
		read_tty_and_console(NULL, NULL, NULL);
		command[3] = 2; // and for second motor
		send_command(command);
		read_tty_and_console(NULL, NULL, NULL);
	}
	if(rb == '+'){
		change_motor_speed(1);
	}else if(rb == '-'){
		change_motor_speed(-1);
	}
	/*
	 * Now we must analize commands: all letters exept Q/q & H/h are for motors commands
	 */
	if(rb > 'a'-1 && rb < 'z'+1){ // small letter -> change motor number to 2
		command[3] = 2;
	}else if(rb > 'A'-1 && rb < 'Z'+1){ // capital letter -> change to small
		rb += 'a' - 'A';
	}else return; // wrong command
	switch(rb){
		case 'r': // move motor right
			*cmd = 1; // ROR
		break;
		case 'l': // move motor left
			*cmd = 2; // ROL
		break;
		case 's': // stop motor
			*cmd = 3; // STP
		break;
		case 'd': // 1/16
			*cmd = 5; // SAP
			command[2] = 140; // ustep resolution
			value = 4;
		break;
		case 'e': // 1/4
			*cmd = 5; // SAP
			command[2] = 140; // ustep resolution
			value = 2;
		break;
		case 'g': // get current position
			*cmd = 6; // GAP
			command[2] = 1; // actual position
		break;
		case 'a': // get everything!
			*cmd = 6; // GAP
			get_information(command);
			return;
		break;
		case 'z': // set to zero current position
			*cmd = 5; // SAP
			command[2] = 1; // actual position
			value = 0;
		break;
		case 'm': // move to relative position
			tcsetattr(STDIN_FILENO, TCSANOW, &oldt); // return terminal to original state
			if(scanf("%d", &V) > 0 && V){
				*cmd = 4; // MVP
				command[2] = 1; // REL
				value = (uint32_t) V;
				//if(V > 0) value = (uint32_t) V;
				//else value =
			}
			tcsetattr(STDIN_FILENO, TCSANOW, &newt); // omit echo
		break;
		case 'w': // wait till reached
			*cmd = 138; // WAIT DIRECT
			// set motor bit mask:
			value = 5; // both motors
			//value = 1 << command[3];
		break;
	}
	if(*cmd){
		// copy param to buffer
		uint8_t *spd = (uint8_t*) &value + 3;
		for(i = 4; i < 8; i++)
			command[i] = *spd--;
		send_command(command);
	}
}

/**
 * Get integer value from buffer
 * @param buff (i) - buffer with int
 * @param len      - length of data in buffer (could be 2 or 4)
 * @return
 *
uint32_t get_int(uint8_t *buff, size_t len){
	if(len != 2 && len != 4){
		fprintf(stdout, "Bad data length!\n");
		return 0xffffffff;
	}
	uint32_t data = 0;
	uint8_t *i8 = (uint8_t*) &data;
	if(len == 2) memcpy(i8, buff, 2);
	else memcpy(i8, buff, 4);
	return data;
}*/

/**
 * Copy line by line buffer buff to file removing cmd starting from newline
 * @param buffer - data to put into file
 * @param cmd - symbol to remove from line startint (if found, change *cmd to (-1)
 * 			or NULL, (-1) if no command to remove
 */
void copy_buf_to_file(uint8_t *buffer, int *cmd){
	uint8_t *buff, *line, *ptr;
	if(!cmd || *cmd < 0){
		fprintf(fout, "%s", buffer);
		return;
	}
	buff = (uint8_t*)strdup((char*)buffer), ptr = buff;
	do{
		if(!*ptr) break;
		if(ptr[0] == (char)*cmd){
			*cmd = -1;
			ptr++;
			if(ptr[0] == '\n') ptr++;
			if(!*ptr) break;
		}
		line = ptr;
		ptr = (uint8_t*)strchr((char*)buff, '\n');
		if(ptr){
			*ptr++ = 0;
			//fprintf(fout, "%s\n", line);
		}//else
			//fprintf(fout, "%s", line); // no newline found in buffer
		fprintf(fout, "%s\n", line);
	}while(ptr);
	free(buff);
}

static inline uint32_t log_2(const uint32_t x) {
	uint32_t y;
	asm ( "\tbsr %1, %0\n"
		: "=r"(y)
		: "r" (x)
	);
	return y;
}

#define log2(x) ((int)log_2((uint32_t)x))

int main(int argc, char *argv[]){
	int rb, oldcmd = -1;
	uint8_t buff[BUFLEN+1];
	size_t L;
	if(argc == 2){
		fout = fopen(argv[1], "a");
		if(!fout){
			perror("Can't open output file");
			exit(-1);
		}
		setbuf(fout, NULL);
	}
	tty_init();
	signal(SIGTERM, quit);		// kill (-15)
	signal(SIGINT, quit);		// ctrl+C
	signal(SIGQUIT, SIG_IGN);	// ctrl+\   .
	signal(SIGTSTP, SIG_IGN);	// ctrl+Z
	setbuf(stdout, NULL);
	//t0 = dtime();
	while(1){
		L = BUFLEN;
		if(read_tty_and_console(buff, &L, &rb)){
			if(rb > 0){
				con_sig(rb);
				oldcmd = rb;
			}
			if(L > 0){
				uint8_t *ptr = buff;
				int ii;
				int32_t val = get_integer(buff);
				const uint8_t pattern[] = {2,1,128,138,0,0,0};
				if(memcmp((void*)buff, (void*)pattern, sizeof(pattern)) == 0){ // motor has reached position
					printf("Motor %d has reached position!\n", log2(val));
				}else{
					printf("value = %d (full answer: ", val);
					for(ii = L - 1; ii > 0; ii--){
						uint8_t C = *ptr++;
						printf("%u", C);
						//if(C > 31) printf("(%c)", C);
						printf(",");
					}
					printf("%u\n", *ptr);
				}
				if(fout){
					copy_buf_to_file(buff, &oldcmd);
				}
			}
		}
	}
}
