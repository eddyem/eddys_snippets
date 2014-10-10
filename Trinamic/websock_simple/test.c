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


#define _U_    __attribute__((__unused__))

int force_exit = 0;

uint8_t buf[9];

char *comdev = "/dev/ttyUSB0";
int BAUD_RATE = B115200;
int comfd = -1; // TTY fd
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

void fillbuf(char *command){
	memset(buf, 0, 9);
	buf[0] = 1; // controller #
	if(command[1] == 'X') buf[3] = 2; // X motor -> #2
	else buf[3] = 0; // Y motor -> #0
	if(command[0] == 'D'){ // start moving
		if(command[2] == '+') buf[1] = 1; // ROR
		else buf[1] = 2; // ROL
	}else{ // stop
		buf[1] = 3; // STP
	}
	buf[7] = 50; // speed = 50
	send_command(buf);
}

void move_motor(char *where){
	printf("moving %s\n", where);

}
void stop_motor(char *where){
	printf("stoping %s\n", where);
}

static int
my_protocol_callback(_U_ struct libwebsocket_context *context,
			_U_ struct libwebsocket *wsi,
			enum libwebsocket_callback_reasons reason,
				_U_ void *user, void *in, _U_ size_t len)
{
	//int n, m;
	//unsigned char buf[LWS_SEND_BUFFER_PRE_PADDING + 512 +
	//					  LWS_SEND_BUFFER_POST_PADDING];
	//unsigned char *p = &buf[LWS_SEND_BUFFER_PRE_PADDING];

	char *msg = (char*) in;

	switch (reason) {

	case LWS_CALLBACK_ESTABLISHED:
	        printf("New Connection\n");
		break;

	case LWS_CALLBACK_SERVER_WRITEABLE:
		break;

	case LWS_CALLBACK_RECEIVE:
		fillbuf(msg);
		break;

	case LWS_CALLBACK_FILTER_PROTOCOL_CONNECTION:
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
		"XY-protocol",			// name
		my_protocol_callback,	// callback
		30,						// per_session_data_size
		10,						//  max frame size / rx buffer
		0, NULL, 0
	},
	{ NULL, NULL, 0, 0, 0, NULL, 0} /* terminator */
};
//**************************************************************************//
void sighandler(_U_ int sig){
	close(comfd);
	force_exit = 1;
}
//**************************************************************************//
//**************************************************************************//
int main(_U_ int argc, _U_ char **argv)
{
	int n = 0;
	struct libwebsocket_context *context;
	int opts = 0;
	const char *iface = NULL;
	int syslog_options = LOG_PID | LOG_PERROR;
	//unsigned int oldus = 0;
	struct lws_context_creation_info info;

	int debug_level = 7;

	memset(&info, 0, sizeof info);
	info.port = 9999;

	signal(SIGINT, sighandler);

	printf("\nOpen port...\n");
	if ((comfd = open(comdev,O_RDWR|O_NOCTTY|O_NONBLOCK)) < 0){
		fprintf(stderr,"Can't use port %s\n",comdev);
		return 1;
	}


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
	if (context == NULL) {
		lwsl_err("libwebsocket init failed\n");
		return -1;
	}

	n = 0;
	while (n >= 0 && !force_exit) {

		n = libwebsocket_service(context, 50);

	};//while n>=0


	libwebsocket_context_destroy(context);

	lwsl_notice("libwebsockets-test-server exited cleanly\n");

	closelog();

	return 0;
}//main
