/*
 * client.c - simple terminal client for operationg with
 * Standa's 8MT175-150 translator by SMSD-1.5 driver
 *
 * Hardware operates in microsterpping mode (1/16),
 * max current = 1.2A
 * voltage     = 12V
 * "0" of driver connected to end-switch at opposite from motor side
 * switch of motor's side connected to "IN1"
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
#include <string.h>			// memcpy, strcmp etc
#include <strings.h>		// strcasecmp
#include <stdint.h>			// int types
#include <sys/time.h>		// gettimeofday
#include "cmdlnopts.h"

#define DBG(...) do{fprintf(stderr, __VA_ARGS__); }while(0)

//double t0; // start time
static int bus_error = 0; // last error of data output
enum{
	NO_ERROR  = 0, // normal execution
	CODE_ERR,      // error of exexuted program code
	BUS_ERR,       // data transmission error
	COMMAND_ERR,   // wrong command
	CMD_DATA_ERR,  // wrong data of command
	UNDEFINED_ERR  // something else wrong
};

int BAUD_RATE = B9600;
uint16_t step_spd = 900;  // stepper speed: 225 steps per second in 1/1 mode
struct termio oldtty, tty; // TTY flags
int comfd = -1; // TTY fd

int erase_ctrlr();

/**
 * Exit & return terminal to old state
 * @param ex_stat - status (return code)
 */
void quit(int ex_stat){
	if(comfd > 0){
		erase_ctrlr();
		ioctl(comfd, TCSANOW, &oldtty ); // return TTY to previous state
		close(comfd);
	}
	printf("Exit! (%d)\n", ex_stat);
	exit(ex_stat);
}

/**
 * Open & setup TTY, terminal
 */
void tty_init(){
	printf("\nOpen port...\n");
	if ((comfd = open(G.comdev,O_RDWR|O_NOCTTY|O_NONBLOCK)) < 0){
		fprintf(stderr,"Can't use port %s\n", G.comdev);
		quit(1);
	}
	printf(" OK\nGet current settings...\n");
	if(ioctl(comfd,TCGETA,&oldtty) < 0) quit(-1); // Get settings
	tty = oldtty;
	tty.c_lflag     = 0; // ~(ICANON | ECHO | ECHOE | ISIG)
	tty.c_oflag     = 0;
	tty.c_cflag     = BAUD_RATE|CS8|CREAD|CLOCAL | PARENB; // 9.6k, 8N1, RW, ignore line ctrl
	tty.c_cc[VMIN]  = 0;  // non-canonical mode
	tty.c_cc[VTIME] = 5;
	if(ioctl(comfd,TCSETA,&tty) < 0) quit(-1); // set new mode
	printf(" OK\n");
}

/**
 * Read data from TTY
 * @param buff (o) - buffer for data read
 * @param length   - buffer len
 * @return amount of readed bytes
 */
size_t read_tty(char *buff, size_t length){
	ssize_t L = 0;
	fd_set rfds;
	struct timeval tv;
	int retval;
	FD_ZERO(&rfds);
	FD_SET(comfd, &rfds);
	tv.tv_sec = 0; tv.tv_usec = 50000;
	retval = select(comfd + 1, &rfds, NULL, NULL, &tv);
	if(retval < 1) return 0;
	if(FD_ISSET(comfd, &rfds)){
		if((L = read(comfd, buff, length)) < 1){
			fprintf(stderr, "ERROR on bus, exit!\n");
			quit(-4);
		}
	}
	return (size_t)L;
}

void write_tty(char *str, int L){
	ssize_t D = write(comfd, str, L);
	if(D != L){
		fprintf(stderr, "ERROR on bus, exit!\n");
		quit(-3);
	}
}

size_t read_ctrl_command(char *buf, size_t L){ // read data from controller to buffer buf
	int i, j;
	char *ptr = buf;
	size_t R;
	memset(buf, 0, L);
	for(j = 0; j < L; j++, ptr++){
		R = 0;
		for(i = 0; i < 10 && !R; i++){
			R = read_tty(ptr, 1);
		}
		if(!R){j--; break;}  // nothing to read
		if(*ptr == '*') // read only one command
			break;
		if(*ptr < ' '){ // omit spaces & non-characters
			j--; ptr--;
		}
	}
	return (size_t) j + 1;
}

int parse_ctrlr_ans(char *ans){
	char *E = NULL, *Star = NULL;
	if(!ans || !*ans) return 1;
	bus_error = NO_ERROR;
	if(!(E = strchr(ans, 'E')) || !(Star = strchr(ans, '*')) || E[1] != '1'){
		fprintf(stderr, "Answer format error (got: %s)\n", ans);
		bus_error = UNDEFINED_ERR;
		return 0;
	}
	switch (E[2]){ // E = "E1x"
		case '0': // 10 - normal execution
		break;
		case '4': // 14 - program end
			printf("Last command exectuted normally\n");
		break;
		case '2': // command interrupt by other signal
			fprintf(stderr, "Last command terminated\n");
		break;
		case '3':
			bus_error = CODE_ERR;
			fprintf(stderr, "runtime");
		break;
		case '5':
			bus_error = BUS_ERR;
			fprintf(stderr, "data bus");
		break;
		case '6':
			bus_error = COMMAND_ERR;
			fprintf(stderr, "command");
		break;
		case '9':
			bus_error = CMD_DATA_ERR;
			fprintf(stderr, "command data");
		break;
		default:
			bus_error = UNDEFINED_ERR;
			fprintf(stderr, "undefined (%s)", ans);
	}
	if(bus_error != NO_ERROR){
		fprintf(stderr, " error in controller\n");
		return 0;
	}
	return 1;
}

int send_command(char *cmd){
	int L = strlen(cmd);
	size_t R = 0;
	char ans[256];
	write_tty(cmd, L);
	R = read_ctrl_command(ans, 255);
//	DBG("readed: %s (cmd: %s, R = %zd, L = %d)\n", ans, cmd, R, L);
	if(!R || (strncmp(ans, cmd, L) != 0)){
		fprintf(stderr, "Error: controller doesn't respond (answer: %s)\n", ans);
		return 0;
	}
	R = read_ctrl_command(ans, 255);
//	DBG("readed: %s\n", ans);
	if(!R){ // controller is running or error
		fprintf(stderr, "Controller doesn't answer!\n");
		return 0;
	}
	return parse_ctrlr_ans(ans);
}

int erase_ctrlr(){
	char *errmsg = "\n\nCan't erase controller's memory: some errors occured!\n\n";
	printf("Erasing old program\n");
	#define safely_send(x) do{ if(bus_error != NO_ERROR){				\
		fprintf(stderr, errmsg); return 0;} send_command(x); }while(0)
	if(!send_command("LD1*")){ // start writing a program
	//if(!send_command("LB*")){ // start writing a program into op-buffer
		if(bus_error == COMMAND_ERR){ // motor is moving
			printf("Found running program, stop it\n");
			if(!send_command("ST1*"))
			//if(!send_command("ST*"))
				send_command("SP*");
			send_command("LD1*");
			//send_command("LB*");
		}else{
			fprintf(stderr, "Controller doesn't answer: maybe no power?\n");
			return 1;
		}
	}
	safely_send("BG*");   // move address pointer to beginning
	safely_send("DS*");   // turn off motor
	safely_send("ED*");   // end of program
	if(bus_error != NO_ERROR){
		fprintf(stderr, errmsg);
		return 0;
	}
	return 1;
}

int con_sig(int rb, int stepsN){
	int got_command = 0;
	char command[256];
	char buf[13];
	#define Die_on_error(arg) do{if(!send_command(arg)) goto erase_;}while(0)
	if(strchr("-+01Rr", rb)){ // command to execute
		got_command = 1;
		if(!send_command("LD1*")){        // start writing a program
		//if(!send_command("LB*")){        // start writing a program into op-buffer
			fprintf(stderr, "Error: previous program is running!\n");
			return 0;
		}
		Die_on_error("BG*");              // move address pointer to beginning
		if(strchr("-+01", rb)){
			Die_on_error("EN*");              // enable power
			//Die_on_error("SD10000*");         // set speed to max (156.25 steps per second with 1/16)
			snprintf(buf, 12, "SD%u*", step_spd);
			Die_on_error(buf);
		}
	}
	switch(rb){
		case '0':
			Die_on_error("DL*");
			Die_on_error("HM*");
		break;
		case '1':
			Die_on_error("DR*");
			Die_on_error("ML*");
		break;
		case '+':
			Die_on_error("DR*");
			if(stepsN)
				sprintf(command, "MV%d*", stepsN);
			else
				sprintf(command, "MV*");
			Die_on_error(command);
		break;
		case '-':
			Die_on_error("DL*");
			if(stepsN)
				sprintf(command, "MV%d*", stepsN);
			else
				sprintf(command, "MV*");
			Die_on_error(command);
		break;
		case 'S':
			Die_on_error("SP*");
		break;
		case 'A':
			Die_on_error("ST1*");
			//Die_on_error("ST*");
		break;
		case 'E':
			erase_ctrlr();
		break;
		case 'R':
			Die_on_error("SF*");
		break;
		case 'r':
			Die_on_error("CF*");
		break;
		case '>': // increase speed for 25 pulses
			step_spd += 25;
			printf("\nCurrent speed: %u pulses per sec\n", step_spd);
			//snprintf(buf, 12, "SD%u*", step_spd);
			//Die_on_error(buf);
		break;
		case '<': // decrease speed for 25 pulses
			if(step_spd > 25){
				step_spd -= 25;
				printf("\nCurrent speed: %u pulses per sec\n", step_spd);
				//snprintf(buf, 12, "SD%u*", step_spd);
				//Die_on_error(buf);
			}else
				printf("\nSpeed is too low\n");
		break;

	}
	if(got_command){ // there was some command: write ending words
		Die_on_error("DS*"); // turn off power from motor at end
		Die_on_error("ED*"); // signal about command end
		Die_on_error("ST1*");// start program
		return 1;
	}
	return 0;
erase_:
	erase_ctrlr();
	return 0;
}

void wait_for_answer(){
	char buff[128], *bufptr = buff;
	size_t L;
	while(1){
		L = read_tty(bufptr, 127);
		if(L){
			bufptr += L;
			if(bufptr - buff > 127){
				fprintf(stderr, "Error: input buffer overflow!\n");
				bufptr = buff;
			}
			if(bufptr[-1] == '*'){ // end of input command
				*bufptr = 0;
				parse_ctrlr_ans(buff);
				return;
			}
		}
	}
}

int main(int argc, char *argv[]){
	parce_args(argc, argv);
	tty_init();
	signal(SIGTERM, quit);		// kill (-15)
	signal(SIGINT,  quit);		// ctrl+C
	signal(SIGQUIT, SIG_IGN);	// ctrl+\   .
	signal(SIGTSTP, SIG_IGN);	// ctrl+Z
	setbuf(stdout, NULL);
	erase_ctrlr();
	if(G.erasecmd) return 0;
	if(G.relaycmd == -1 && G.gotopos == NULL){
		printf("No commands given!\n");
		return -1;
	}
	if(G.relaycmd != -1){
		int ans;
		if(G.relaycmd) // turn on
			ans = con_sig('R',0);
		else // turn off
			ans = con_sig('r',0);
		if(ans)
			wait_for_answer();
		else
			return -1;
	}
	if(G.gotopos){
		if(strcasecmp(G.gotopos, "refmir") == 0){
			if(!con_sig('1',0)) return -1;
			printf("Go to last end-switch\n");
			wait_for_answer();
			if(!con_sig('-',500)) return -1;
		}else if(strcasecmp(G.gotopos, "diagmir") == 0){
			if(!con_sig('0',0)) return -1;
			printf("Go to zero's end-switch\n");
			wait_for_answer();
			if(!con_sig('+',2500)) return -1;
		}else if(strcasecmp(G.gotopos, "shack") == 0){
			if(!con_sig('1',0)) return -1;
			printf("Go to last end-switch\n");
			wait_for_answer();
			if(!con_sig('-',30000)) return -1;
		}else{
			printf("Wrong goto command, should be one of refmir/diagmir/shack\n");
			return -1;
		}
		printf("Go to position\n");
		wait_for_answer();
	}
	return 0;
}
