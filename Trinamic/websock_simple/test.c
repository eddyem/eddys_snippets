#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <syslog.h>

#include <signal.h>

#include <libwebsockets.h>

#include <termios.h>		// tcsetattr
#include <stdint.h>			// int types
#include <sys/stat.h>		// read
#include <fcntl.h>			// read

#include <pthread.h>

#define _U_    __attribute__((__unused__))

#define MESSAGE_QUEUE_SIZE 3
#define MESSAGE_LEN        128
// individual data per session
typedef struct{
	int num;
	int idxwr;
	int idxrd;
	char message[MESSAGE_QUEUE_SIZE][MESSAGE_LEN];
}per_session_data;

per_session_data global_queue;
pthread_mutex_t command_mutex;

char cmd_buf[5] = {0};
int data_in_buf = 0; // signals that there's some data in cmd_buf to send to motors

void put_message_to_queue(char *msg, per_session_data *dat){
	int L = strlen(msg);
	if(dat->num >= MESSAGE_QUEUE_SIZE) return;
	dat->num++;
	if(L < 1 || L > MESSAGE_LEN - 1) L = MESSAGE_LEN - 1;
	strncpy(dat->message[dat->idxwr], msg, L);
	dat->message[dat->idxwr][L] = 0;
	if((++(dat->idxwr)) >= MESSAGE_QUEUE_SIZE) dat->idxwr = 0;
}

char *get_message_from_queue(per_session_data *dat){
	char *R = dat->message[dat->idxrd];
	if(dat->num <= 0) return NULL;
	if((++dat->idxrd) >= MESSAGE_QUEUE_SIZE) dat->idxrd = 0;
	dat->num--;
	return R;
}

int force_exit = 0;

uint8_t buf[9];
#define TTYBUFLEN 128
uint8_t ttybuff[TTYBUFLEN];
char *comdev = "/dev/ttyUSB0";
int BAUD_RATE = B115200;
int comfd = -1; // TTY fd
uint32_t motor_speed = 50;

//**************************************************************************//
int32_t get_integer(uint8_t *buff){
	int32_t val;
	int ii;
	uint8_t *valptr = (uint8_t*) &val + 3;
	for(ii = 4; ii < 8; ii++)
		*valptr-- = buff[ii];
	return val;
}
void inttobuf(uint8_t *buf, int32_t value){// copy param to buffer
	int i;
	uint8_t *bytes = (uint8_t*) &value + 3;
	for(i = 4; i < 8; i++)
		buf[i] = *bytes--;
}

/**
 * read tty
 * @return number of readed symbols
 */
size_t read_tty(){
	ssize_t L = 0, l, buffsz = TTYBUFLEN;
	struct timeval tv;
	int sel;
	fd_set rfds;
	FD_ZERO(&rfds);
	FD_SET(comfd, &rfds);
	tv.tv_sec = 0; tv.tv_usec = 100000;
	sel = select(comfd + 1, &rfds, NULL, NULL, &tv);
	if(sel > 0){
		if(FD_ISSET(comfd, &rfds)){
			if((L = read(comfd, ttybuff, buffsz)) < 1){ // disconnect or other troubles
				fprintf(stderr, "USB error or disconnected!\n");
				exit(1);
			}else{
				if(L == 0){ // USB disconnected
					fprintf(stderr, "USB disconnected!\n");
					exit(1);
				}
				if(L == 9) return 9;
				// all OK continue reading
				//DBG("readed %zd bytes, try more.. ", L);
				buffsz -= L;
				select(comfd + 1, &rfds, NULL, NULL, &tv);
				while(L < 9 && buffsz > 0 && (l = read(comfd, ttybuff+L, buffsz)) > 0){
					L += l;
					buffsz -= l;
					select(comfd + 1, &rfds, NULL, NULL, &tv);
				}
			}
		}
	}
	return (size_t) L;
}

static inline uint32_t log_2(const uint32_t x){
    if(x == 0) return 0;
    return (31 - __builtin_clz (x));
}

int send_command(uint8_t *ninebytes);

#define log2(x) ((int)log_2((uint32_t)x))
/*
 * check L bytes of ttybuf (maybe there was a command to check endpoint)
 */
void check_tty_sig(size_t l){
	int L = l;
	uint8_t movbk[]= {1,4,1,0,0,0,0,0,0};
	//if(L < 9) return; // WTF?
	uint8_t* buf = ttybuff;
	char msg[128];
	const uint8_t pattern[] = {2,1,128,138,0,0,0};
	while(L > 0){
		int32_t Ival = get_integer(buf);
		if(memcmp((void*)buf, (void*)pattern, sizeof(pattern)) == 0){ // motor has reached position
			int motnum = log2(Ival);
			snprintf(msg, 127, "Motor %d has reached position!", motnum);
			printf(" %s (%d)\n", msg, Ival);
			put_message_to_queue(msg, &global_queue);
			if(motnum == 0){ // move motor 0 to 2000 usteps
				inttobuf(movbk, 2000);
				send_command(movbk);
			}else if(motnum == 2){ // move motor 2 to 3450 usteps
				movbk[3] = 2;
				inttobuf(movbk, 3450);
				send_command(movbk);
			}else{
				printf(" WTF?\n");
			}
		}else printf(" %d\n", Ival);
		L -= 9;
		buf += 9;
	};
}

int send_command(uint8_t *ninebytes){
	uint8_t crc = 0;
	size_t L;
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
	if((L = read_tty())){
		printf("got %zd bytes from tty: ", L);
		check_tty_sig(L);
	}
	return 1;
}

void process_buf(char *command){
	memset(buf, 0, 9);
	buf[0] = 1; // controller #
	if(command[0] == 'W'){ // 1/16
		buf[1] = 5;
		buf[2] = 140; // ustep resolution
		buf[7] = 4; // 1/16
		send_command(buf);
		buf[3] = 2; // motor #2
		send_command(buf);
		return;
	}else if(command[0] == 'S'){ // change current speed
		long X = strtol(&command[1], NULL, 10);
		if(X > 9 && X < 501){
			motor_speed = (uint32_t) X;
			printf("set speed to %u\n", motor_speed);
			buf[1] = 5; // SAP
			buf[2] = 4; // max pos speed
			inttobuf(buf, motor_speed);
			send_command(buf);
			buf[3] = 2; // 2nd motor
			send_command(buf);
		}
		return;
	}
	if(command[1] == '0'){ // go to start point for further moving to middle
		if(command[0] == 'U') return;
		uint8_t wt[] = {1,138,0,0,0,0,0,5,0};
		uint8_t movbk[]= {1,4,1,0,0,0,0,0,0};
		inttobuf(movbk, -4500); // 1st motor
		send_command(movbk);
		movbk[3] = 2;
		inttobuf(movbk, -7300); // 2nd motor
		send_command(movbk);
		send_command(wt); // wait
		return;
	}
	if(command[1] == 'X') buf[3] = 2; // X motor -> #2
	else if(command[1] == 'Y') buf[3] = 0; // Y motor -> #0
	if(command[0] == 'D'){ // start moving
		if(command[2] == '+') buf[1] = 1; // ROR
		else if(command[2] == '-') buf[1] = 2; // ROL
	}else if(command[0] == 'U'){ // stop
		buf[1] = 3; // STP
	}
	inttobuf(buf, motor_speed);
	if(!send_command(buf)){
		printf("Can't send command");
	};
}

#define MESG(X) do{if(dat) put_message_to_queue(X, dat);}while(0)
void websig(char *command, per_session_data *dat){
	if(command[0] == 'W' || command[1] == '0' || command[0] == 'S'){
		if(command[0] == 'W'){
			MESG("Set microstepping to 1/16");
		}else if(command[1] == '0'){
			MESG("Go to the middle. Please, wait!");
		}else{
			MESG("Change speed");
		}
		goto ret;
	}
	if(command[1] != 'X' && command[1] != 'Y'){ // error
		MESG("Undefined coordinate");
		return;
	}
	if(command[0] != 'D' && command[0] != 'U'){
		MESG("Undefined command");
		return;
	}
ret:
	pthread_mutex_lock(&command_mutex);
	strncpy(cmd_buf, command, 4);
	data_in_buf = 1;
	pthread_mutex_unlock(&command_mutex);
	while(data_in_buf); // wait for execution
}

static void dump_handshake_info(struct libwebsocket *wsi){
	int n;
	static const char *token_names[] = {
		"GET URI",
		"POST URI",
		"OPTIONS URI",
		"Host",
		"Connection",
		"key 1",
		"key 2",
		"Protocol",
		"Upgrade",
		"Origin",
		"Draft",
		"Challenge",

		/* new for 04 */
		"Key",
		"Version",
		"Sworigin",

		/* new for 05 */
		"Extensions",

		/* client receives these */
		"Accept",
		"Nonce",
		"Http",

		/* http-related */
		"Accept:",
		"Ac-Request-Headers:",
		"If-Modified-Since:",
		"If-None-Match:",
		"Accept-Encoding:",
		"Accept-Language:",
		"Pragma:",
		"Cache-Control:",
		"Authorization:",
		"Cookie:",
		"Content-Length:",
		"Content-Type:",
		"Date:",
		"Range:",
		"Referer:",
		"Uri-Args:",


		"MuxURL",

		/* use token storage to stash these */

		"Client sent protocols",
		"Client peer address",
		"Client URI",
		"Client host",
		"Client origin",

		/* always last real token index*/
		"WSI token count"
	};
	char buf[256];
	int L = sizeof(token_names) / sizeof(token_names[0]);
	for (n = 0; n < L; n++) {
		if (!lws_hdr_total_length(wsi, n))
			continue;
		lws_hdr_copy(wsi, buf, sizeof buf, n);
		printf("    %s = %s\n", token_names[n], buf);
	}
}

static int my_protocol_callback(_U_ struct libwebsocket_context *context,
			_U_ struct libwebsocket *wsi,
			enum libwebsocket_callback_reasons reason,
				_U_ void *user, void *in, _U_ size_t len){
	unsigned char buf[LWS_SEND_BUFFER_PRE_PADDING + MESSAGE_LEN +
				  LWS_SEND_BUFFER_POST_PADDING];
	unsigned char *p = &buf[LWS_SEND_BUFFER_PRE_PADDING];
	char client_name[128];
	char client_ip[128];
	char *M, *msg = (char*) in;
	per_session_data *dat = (per_session_data *) user;
	int L, W;
	void parse_queue_msg(per_session_data *d){
		if((M = get_message_from_queue(d))){
			L = strlen(M);
			strncpy((char *)p, M, L);
			W = libwebsocket_write(wsi, p, L, LWS_WRITE_TEXT);
			if(L != W){
				lwsl_err("Can't write to socket");
			}
		}
	}
	//struct lws_tokens *tok = (struct lws_tokens *) user;
	switch (reason) {
		case LWS_CALLBACK_ESTABLISHED:
			printf("New Connection\n");
			memset(dat, 0, sizeof(per_session_data));
			libwebsocket_callback_on_writable(context, wsi);
		break;
		case LWS_CALLBACK_SERVER_WRITEABLE:
			if(dat->num == 0 && global_queue.num == 0){
				libwebsocket_callback_on_writable(context, wsi);
				return 0;
			}else{
				parse_queue_msg(dat);
				parse_queue_msg(&global_queue);
				libwebsocket_callback_on_writable(context, wsi);
			}
		break;
		case LWS_CALLBACK_RECEIVE:
			websig(msg, dat);
		break;
		case LWS_CALLBACK_FILTER_NETWORK_CONNECTION:
			libwebsockets_get_peer_addresses(context, wsi, (int)(long)in,
				client_name, 127, client_ip, 127);
			printf("Received network connection from %s (%s)\n",
							client_name, client_ip);
		break;
		case LWS_CALLBACK_FILTER_PROTOCOL_CONNECTION:
			printf("Client asks for %s\n", msg);
			dump_handshake_info(wsi);
		break;
		case LWS_CALLBACK_CLOSED:
			printf("Client disconnected\n");
		break;
		default:
		break;
	}

	return 0;
}

//**************************************************************************//
/* list of supported protocols and callbacks */
//**************************************************************************//
static struct libwebsocket_protocols protocols[] = {
	/* first protocol must always be HTTP handler */

	{
		"XY-protocol",				// name
		my_protocol_callback,		// callback
		sizeof(per_session_data),	// per_session_data_size
		10,							// max frame size / rx buffer
		0, NULL, 0
	},
	{ NULL, NULL, 0, 0, 0, NULL, 0} /* terminator */
};

//**************************************************************************//
void sighandler(_U_ int sig){
	close(comfd);
	force_exit = 1;
}

void *websock_thread(_U_ void *buf){
	struct libwebsocket_context *context;
	int n = 0;
	int opts = 0;
	const char *iface = NULL;
	int syslog_options = LOG_PID | LOG_PERROR;
	//unsigned int oldus = 0;
	struct lws_context_creation_info info;
	int debug_level = 7;

	if(pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL)){
		force_exit = 1;
		return NULL;
	}

	memset(&info, 0, sizeof info);
	info.port = 9999;

	/* we will only try to log things according to our debug_level */
	setlogmask(LOG_UPTO (LOG_DEBUG));
	openlog("lwsts", syslog_options, LOG_DAEMON);

	/* tell the library what debug level to emit and to send it to syslog */
	lws_set_log_level(debug_level, lwsl_emit_syslog);

	info.iface = iface;
	info.protocols = protocols;
	info.extensions = libwebsocket_get_internal_extensions();
	info.ssl_cert_filepath = NULL;
	info.ssl_private_key_filepath = NULL;

	info.gid = -1;
	info.uid = -1;
	info.options = opts;

	context = libwebsocket_create_context(&info);
	if (context == NULL){
		lwsl_err("libwebsocket init failed\n");
		force_exit = 1;
		return NULL;
	}

	while(n >= 0 && !force_exit){
		n = libwebsocket_service(context, 500);
	}//while n>=0
	libwebsocket_context_destroy(context);
	lwsl_notice("libwebsockets-test-server exited cleanly\n");
	closelog();
	return NULL;
}

//**************************************************************************//
int main(_U_ int argc, _U_ char **argv){
	pthread_t w_thread;
	size_t L;

	signal(SIGINT, sighandler);

	if(argc == 2) comdev = argv[1];

	printf("\nOpen port %s...\n", comdev);
	if ((comfd = open(comdev,O_RDWR|O_NOCTTY|O_NONBLOCK)) < 0){
		fprintf(stderr,"Can't use port %s\n",comdev);
		return 1;
	}

	process_buf("W"); // step: 1/16

	pthread_create(&w_thread, NULL, websock_thread, NULL);

	while(!force_exit){
		if((L = read_tty())){
			printf("got %zd bytes from tty:\n", L);
			check_tty_sig(L);
		}
		pthread_mutex_lock(&command_mutex);
		if(data_in_buf) process_buf(cmd_buf);
		data_in_buf = 0;
		pthread_mutex_unlock(&command_mutex);
	}
	pthread_join(w_thread, NULL); // wait for closing of libsockets thread
	return 0;
}//main
