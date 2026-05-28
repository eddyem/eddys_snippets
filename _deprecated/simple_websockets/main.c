/*
 * main.c
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <libwebsockets.h>
#include <pthread.h>
#include <sys/prctl.h>
#include <time.h>
#include <openssl/md5.h>

#include "dbg.h"
#include "que.h"

#define MESSAGE_LEN        (512)
#define FRAME_SIZE         (LWS_SEND_BUFFER_PRE_PADDING + MESSAGE_LEN + LWS_SEND_BUFFER_POST_PADDING)


char *client_IP = NULL; // IP of first connected client
volatile int stop = 0;
const char *passhash = "1a1dc91c907325c69271ddf0c944bc72"; // "pass"
char *pwencrypted;

pthread_mutex_t command_mutex, ip_mutex;

static void interrupt(_U_ int signo) {
	stop = 1;
}


int ws_write(struct lws *wsi_in, char *str){
	if (!str || !wsi_in)
		return -1;
	unsigned char buf[FRAME_SIZE];
	int n;
	size_t len = strlen(str);
	if(len > MESSAGE_LEN) len = MESSAGE_LEN;
	memcpy(buf + LWS_SEND_BUFFER_PRE_PADDING, str, len);
	n = lws_write(wsi_in, buf + LWS_SEND_BUFFER_PRE_PADDING, len, LWS_WRITE_TEXT);
	//DBG("write %s\n", str);
	return n;
}

void websig(char *command, control_data *dat){
	/*while(data_in_buf);
	pthread_mutex_lock(&command_mutex);
	strncpy(cmd_buf, command, CMDBUFLEN);
	data_in_buf = 1;
	pthread_mutex_unlock(&command_mutex);*/
	DBG("got message: %s\n", command);
	if(dat->state == SALT_SENT){
		if(strcmp(command, pwencrypted) == 0){ // pass OK
			dat->state = VERIFIED;
			put_message_to_que("authOK", dat);
		}else{ // wrong password
			put_message_to_que("badpass", dat);
		}
	}
}

char *str2md5(const char *str) {
	int n;
	MD5_CTX c;
	unsigned char digest[16];
	static char out[33] = {0};
	size_t length = strlen(str);
	if(length > 512) length = 512;
	MD5_Init(&c);
	MD5_Update(&c, str, length);
	MD5_Final(digest, &c);
	for (n = 0; n < 16; ++n) {
		snprintf(&(out[n*2]), 3, "%02x", (unsigned int)digest[n]);
	}
	return out;
}


char *gensalt(){
	static char buf[38] = "pwhash";
	char salt_and_pass[64];
	int i;
	for(i = 6; i < 38; ++i){
		char r = rand() % 26;
		if(rand() & 1)
			r += 'a';
		else
			r += 'A';
		buf[i] = r;
	}
	DBG("gen: %s", buf);
	strcpy(salt_and_pass, &buf[6]);
	strcat(salt_and_pass, passhash);
	pwencrypted = str2md5(salt_and_pass);
	DBG("salt + pass: %s", pwencrypted);
	return buf;
}

static int control_callback(struct lws *wsi,
							enum lws_callback_reasons reason, void *user,
							void *in, size_t len){
	char client_name[128];
	char client_ip[128];
	control_data *dat = (control_data *) user;
	char *msg = (char*) in;
	void parse_que_msg(control_data *d){
		char *M;
		if((M = get_message_from_que(d))){
			ws_write(wsi, M);
		}
	}
    switch (reason) {
		case LWS_CALLBACK_ESTABLISHED:
			memset(dat, 0, sizeof(control_data));
			pthread_mutex_lock(&ip_mutex);
			lws_get_peer_addresses(wsi, lws_get_socket_fd(wsi),
						client_name, 128, client_ip, 128);
			DBG("client %s connected from IP %s", client_name, client_ip);
			if(!client_IP){
				client_IP = strdup(client_ip);
				DBG("new connection");
				dat->state = SALT_SENT;
				put_message_to_que(gensalt(), dat); // send "pwhash" + salt
			}else{
				char buf[256];
				snprintf(buf, 255, "Already connected from %s. Please, disconnect.", client_IP);
				DBG("Already connected\n");
				put_message_to_que(buf, dat);
				snprintf(buf, 255, "Try of connection from %s", client_ip);
				glob_que(buf);
				dat->already_connected = 1;
			}
			pthread_mutex_unlock(&ip_mutex);
			lws_callback_on_writable(wsi);
		break;
		case LWS_CALLBACK_SERVER_WRITEABLE:
			if(dat->num)
				parse_que_msg(dat);
			if(dat->state == VERIFIED && !dat->already_connected && global_que.num)
					parse_que_msg(&global_que);
			usleep(500);
			lws_callback_on_writable(wsi);
			return 0;
		break;
		case LWS_CALLBACK_RECEIVE:
			if(!dat->already_connected)
				websig(msg, dat);
		break;
		case LWS_CALLBACK_CLOSED:
			DBG("closed\n");
			if(!dat->already_connected){
				pthread_mutex_lock(&ip_mutex);
				free(client_IP);
				client_IP = NULL;
				pthread_mutex_unlock(&ip_mutex);
			}
		break;
	}
	return 0;
}

struct lws_protocols protocols[] = {
	{
		"IRBIS_control",			// name
		control_callback,			// callback
		sizeof(control_data),		// control_data_size
		FRAME_SIZE,					// max frame size / rx buffer
		0, NULL						// id, user
	},
	{ NULL, NULL, 0, 0, 0, NULL} /* terminator */
};

void *websock_thread(_U_ void *buf){
	struct lws_context_creation_info info;
	struct lws_context *context;

	memset(&info, 0, sizeof info);
	info.port = 9999;
	info.iface = NULL;
	info.protocols = protocols;
	info.extensions = lws_get_internal_extensions();
	info.ssl_cert_filepath = NULL;
	info.ssl_private_key_filepath = NULL;
	info.gid = -1;
	info.uid = -1;
	info.options = 0;

	context = lws_create_context(&info);
	if (context == NULL) {
		lwsl_err("libwebsocket init failed\n");
		stop = 1;
	}
	while (!stop) {
		lws_service(context, 50);
	}
	usleep(10);
	lws_context_destroy(context);
	return NULL;
}

void *worker_thread(_U_ void* buf){
	time_t t = time(NULL), oldt;
	while(!stop){
		oldt = t;
		if(client_IP){
			glob_que(ctime(&t));
		}
		while((t = time(NULL)) == oldt) usleep(1000);
	}
}

static void main_proc(){
	pthread_t ws_thread, wr_thread;
	if(pthread_create(&ws_thread, NULL, websock_thread, NULL)){
		fprintf(stderr, "Can't create websocket thread!\n");
		stop = 1;
		return;
	}
	if(pthread_create(&wr_thread, NULL, worker_thread, NULL)){
		fprintf(stderr, "Can't create worker thread!\n");
		stop = 1;
		return;
	}
	while(!stop){
	/*	pthread_mutex_lock(&command_mutex);
		if(data_in_buf) process_buf(cmd_buf);
		data_in_buf = 0;
		pthread_mutex_unlock(&command_mutex);*/
		usleep(1000); // give another treads some time to fill buffer
	}
	DBG("stop threads");
	pthread_cancel(ws_thread);
	pthread_cancel(wr_thread);
	pthread_join(ws_thread, NULL);
	pthread_join(wr_thread, NULL);
	DBG("return main_proc");
}

int main(void) {
	signal(SIGINT, interrupt);
	signal(SIGTERM, interrupt);
	signal(SIGQUIT, SIG_IGN);
	signal(SIGTSTP, SIG_IGN);
	srand(time(NULL));
	while(1){
		if(stop) return 0;
		pid_t childpid = fork();
		if(childpid){
			DBG("Created child with PID %d\n", childpid);
			wait(NULL);
			DBG("Child %d died\n", childpid);
		}else{
			prctl(PR_SET_PDEATHSIG, SIGTERM); // send SIGTERM to child when parent dies
			main_proc();
			return 0;
		}
	}
	return 0;
}
