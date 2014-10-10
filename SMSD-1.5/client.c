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
#include <stdint.h>			// int types
#include <sys/time.h>		// gettimeofday

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

FILE *fout = NULL; // file for messages duplicating
char *comdev = "/dev/ttyUSB0";
int BAUD_RATE = B9600;
struct termio oldtty, tty; // TTY flags
struct termios oldt, newt; // terminal flags
int comfd = -1; // TTY fd

int erase_ctrlr();

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
	if(comfd > 0){
		erase_ctrlr();
		ioctl(comfd, TCSANOW, &oldtty ); // return TTY to previous state
		close(comfd);
	}
	if(fout) fclose(fout);
	printf("Exit! (%d)\n", ex_stat);
	exit(ex_stat);
}

/**
 * Open & setup TTY, terminal
 */
void tty_init(){
	// terminal without echo
	tcgetattr(STDIN_FILENO, &oldt);
	newt = oldt;
	//newt.c_lflag &= ~(ICANON | ECHO);
	newt.c_lflag &= ~(ICANON);
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
	tty.c_cflag     = BAUD_RATE|CS8|CREAD|CLOCAL | PARENB; // 9.6k, 8N1, RW, ignore line ctrl
	tty.c_cc[VMIN]  = 0;  // non-canonical mode
	tty.c_cc[VTIME] = 5;
	if(ioctl(comfd,TCSETA,&tty) < 0) quit(-1); // set new mode
	printf(" OK\n");
}

/**
 * Read characters from console without echo
 * @return char readed
 */
int read_console(char *buf, size_t len){
	int rb = 0;
	ssize_t L = 0;
	struct timeval tv;
	int retval;
	fd_set rfds;
	tcsetattr(STDIN_FILENO, TCSANOW, &newt);
	FD_ZERO(&rfds);
	FD_SET(STDIN_FILENO, &rfds);
	tv.tv_sec = 0; tv.tv_usec = 1000;
	retval = select(1, &rfds, NULL, NULL, &tv);
	if(retval){
		if(FD_ISSET(STDIN_FILENO, &rfds)){
			if(len < 2 || !buf) // command works as simple getchar
				rb = getchar();
			else{ // read all data from console buffer
				if((L = read(STDIN_FILENO, buf, len)) > 0) rb = (int)L;
			}
		}
	}
	//tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
	return rb;
}

/**
 * getchar() without echo
 * wait until at least one character pressed
 * @return character readed
 */
int mygetchar(){ // аналог getchar() без необходимости жать Enter
	int ret;
	do ret = read_console(NULL, 1);
	while(ret == 0);
	return ret;
}

/**
 * Read data from TTY
 * @param buff (o) - buffer for data read
 * @param length   - buffer len
 * @return amount of readed bytes
 */
size_t read_tty(uint8_t *buff, size_t length){
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
			exit(-4);
		}
	}
	return (size_t)L;
}

/**
 * wait for answer from server
 * @param sock - socket fd
 * @return 0 in case of error or timeout, 1 in case of socket ready
 *
int waittoread(int sock){
	fd_set fds;
	struct timeval timeout;
	int rc;
	timeout.tv_sec = 0;
	timeout.tv_usec = 1000;
	FD_ZERO(&fds);
	FD_SET(sock, &fds);
	rc = select(sock+1, &fds, NULL, NULL, &timeout);
	if(rc < 0){
		perror("select failed");
		return 0;
	}
	if(rc > 0 && FD_ISSET(sock, &fds)) return 1;
	return 0;
}
*/

void help(){
	printf("\n\nUse this commands:\n"
		"0\tMove to end-switch 0\n"
		"1\tMove to end-switch 1\n"
		"Lxxx\tMake xxx steps toward zero's end-switch (0 main infinity)\n"
		"Rxxx\tMake xxx steps toward end-switch 1      (0 main infinity)\n"
		"S\tStop/start motor when program is running\n"
		"A\tRun previous command again or stop when running\n"
		"E\tErase previous program from controller's memory\n"
		"\n"
	);
}

void write_tty(char *str, int L){
	ssize_t D = write(comfd, str, L);
	if(D != L){
		fprintf(stderr, "ERROR on bus, exit!\n");
		exit(-3);
	}
}

#define dup_pr(...) do{printf(__VA_ARGS__); if(fout) fprintf(fout, __VA_ARGS__);}while(0)

size_t read_ctrl_command(char *buf, size_t L){ // read data from controller to buffer buf
	int i, j;
	char *ptr = buf;
	size_t R;
	memset(buf, 0, L);
	for(j = 0; j < L; j++, ptr++){
		R = 0;
		for(i = 0; i < 10 && !R; i++){
			R = read_tty((uint8_t*)ptr, 1);
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

int send_command(char *cmd){
	int L = strlen(cmd);
	size_t R = 0;
	char ans[256];
	write_tty(cmd, L);
	R = read_ctrl_command(ans, 255);
	DBG("readed: %s (cmd: %s, R = %zd, L = %d)\n", ans, cmd, R, L);
	if(!R || (strncmp(ans, cmd, L) != 0)){
		fprintf(stderr, "Error: controller doesn't respond (answer: %s)\n", ans);
		return 0;
	}
	R = read_ctrl_command(ans, 255);
	DBG("readed: %s\n", ans);
	if(!R){ // controller is running or error
		fprintf(stderr, "Controller doesn't answer!\n");
		return 0;
	}
	bus_error = NO_ERROR;
	//if(strncasecmp(ptr, "HM")
	//else
	if(		strncmp(ans, "E10*", 4) != 0 &&
			strncmp(ans, "E14*", 4) != 0 &&
			strncmp(ans, "E12*", 4) != 0){
		fprintf(stderr, "Error in controller: ");
		if(strncmp(ans, "E13*", 4) == 0){
			bus_error = CODE_ERR;
			fprintf(stderr, "runtime");
		}else if(strncmp(ans, "E15*", 4) == 0){
			bus_error = BUS_ERR;
			fprintf(stderr, "databus");
		}else if(strncmp(ans, "E16*", 4) == 0){
			bus_error = COMMAND_ERR;
			fprintf(stderr, "command");
		}else if(strncmp(ans, "E19*", 4) == 0){
			bus_error = CMD_DATA_ERR;
			fprintf(stderr, "command data");
		}else{
			bus_error = UNDEFINED_ERR;
			fprintf(stderr, "undefined (%s)", ans);
		}
		fprintf(stderr,  " error\n");
		return 0;
	}
	DBG("ALL OK\n");
	return 1;
}
/*
int send5times(char *cmd){ // sends command 'cmd' up to 5 times (if errors), return 0 in case of false
	int N, R = 0;
	for(N = 0; N < 5 && !R; N++){
		R = send_command(cmd);
	}
	return R;
}*/


int erase_ctrlr(){
	char *errmsg = "\n\nCan't erase controller's memory: some errors occured!\n\n";
	#define safely_send(x) do{ if(bus_error != NO_ERROR){				\
		fprintf(stderr, errmsg); return 0;} send_command(x); }while(0)
	if(!send_command("LD1*")){ // start writing a program
		if(bus_error == COMMAND_ERR){ // motor is moving
			printf("Found running program, stop it\n");
			if(send_command("ST1*"))
				send_command("LD1*");
		}else{
			fprintf(stderr, "Controller doesn't answer: try to press S or E\n");
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

void con_sig(int rb){
	int stepsN = 0, got_command = 0;
	char command[256];
	if(rb < 1) return;
	if(rb == 'q') quit(0); // q == exit
	if(rb == 'L' || rb == 'R'){
		if(!fgets(command, 255, stdin)){
			fprintf(stderr, "You should give amount of steps after commands 'L' and 'R'\n");
			return;
		}
		stepsN = atoi(command) * 16; // microstepping
		if(stepsN < 0 || stepsN > 10000000){
			fprintf(stderr, "\n\nSteps amount should be > -1 and < 625000 (0 means infinity)!\n\n");
			return;
		}
	}
	#define Die_on_error(arg) do{if(!send_command(arg)) goto erase_;}while(0)
	if(strchr("LR01", rb)){ // command to execute
		got_command = 1;
		if(!send_command("LD1*")){        // start writing a program
			fprintf(stderr, "Error: previous program is running!\n");
			return;
		}
		Die_on_error("BG*");              // move address pointer to beginning
		Die_on_error("EN*");              // enable power
		Die_on_error("SD10000*");         // set speed to max (625 steps per second with 1/16)
	}
	switch(rb){
		case 'h':
			help();
		break;
		case '0':
			Die_on_error("DL*");
			Die_on_error("HM*");
		break;
		case '1':
			Die_on_error("DR*");
			Die_on_error("ML*");
		break;
		case 'R':
			Die_on_error("DR*");
			if(stepsN)
				sprintf(command, "MV%d*", stepsN);
			else
				sprintf(command, "MV*");
			Die_on_error(command);
		break;
		case 'L':
			Die_on_error("DL*");
			if(stepsN)
				sprintf(command, "MV%d*", stepsN);
			else
				sprintf(command, "MV*");
			Die_on_error(command);
		break;
		case 'S':
			Die_on_error("PS1*");
		break;
		case 'A':
			Die_on_error("ST1*");
		break;
		case 'E':
			erase_ctrlr();
		break;
/*		default:
			cmd = (uint8_t) rb;
			write(comfd, &cmd, 1);*/
	}
	if(got_command){ // there was some command: write ending words
		Die_on_error("DS*"); // turn off power from motor at end
		Die_on_error("ED*"); // signal about command end
		Die_on_error("ST1*");// start program
	}
	return;
erase_:
	if(!erase_ctrlr()) quit(1);
}

/**
 * Get integer value from buffer
 * @param buff (i) - buffer with int
 * @param len      - length of data in buffer (could be 2 or 4)
 * @return
 *
uint32_t get_int(uint8_t *buff, size_t len){
	int i;
	printf("read %zd bytes: ", len);
	for(i = 0; i < len; i++) printf("0x%x ", buff[i]);
	printf("\n");
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

int main(int argc, char *argv[]){
	int rb;
	uint8_t buff[128];
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
	if(!erase_ctrlr()) quit(1);
	//t0 = dtime();
	while(1){
		rb = read_console(NULL, 1);
		if(rb > 0) con_sig(rb);
		L = read_tty(buff, 127);
		if(L){
			buff[L] = 0;
			printf("%s", buff);
			if(fout) fprintf(fout, "%zd\t%s\n", time(NULL), buff);
		}
	}
}
