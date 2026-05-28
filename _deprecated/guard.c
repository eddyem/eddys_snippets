#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>

int child_killed = 0;
pthread_t athread;

void sighndlr(int sig){
	printf("got signal %d\n", sig);
	if(sig == SIGCHLD){
		printf("Child was killed\n");
		child_killed = 1;
	}else if(sig == SIGSEGV){
		printf("suicide\n");
		pthread_exit(NULL);
	}else
		exit(sig);
}

void dummy(){
	int i;
	char *ptr = malloc(10000);
	for(i = 0; i < 256; ++i, ptr += 1024){
		*ptr += i;
	}
}

pid_t create_child(){
	child_killed = 0;
	pid_t p = fork();
	if(p == 0){
		printf("child\n");
		dummy();
	}
	return p;
}

void *thrfnctn(void *buf){
	dummy();
}

int main(){
	int i;
	//for(i = 0; i < 255; ++i)
	//	signal(i, sighndlr);
	signal(SIGCHLD, sighndlr);
	signal(SIGSEGV, sighndlr);
	for(i = 0; i < 10; ++i){
		pid_t p = create_child();
		if(p){
			printf("child #%d with PID %d\n", i, p);
			while(!child_killed)
				usleep(10000);
		}else return 0;
	}
	for(i = 0; i < 10; ++i){
		if(pthread_create(&athread, NULL, thrfnctn, NULL))
			return -1;
		printf("thread #%d\n", i);
		while(pthread_kill(athread, 0) != ESRCH)
			usleep(1000);
		pthread_join(athread, NULL);
	}
	return 0;
}

