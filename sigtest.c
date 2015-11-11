#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/prctl.h>
#include <signal.h>
#include <unistd.h>

volatile pid_t childpid = 0;

void signal_handler(int sig){
	printf("PID %zd got signal %d, die\n", getpid(), sig);
	if(childpid)
		kill(childpid, SIGTERM);
	exit(sig);
}

pid_t create_child(){
	int i;
	pid_t p = fork();
	if(p == 0){
		prctl(PR_SET_PDEATHSIG, SIGTERM); // send SIGTERM to child when parent dies
		printf("child\n");
		sleep(1);
	}
	return p;
}

int main(){
	int i = 0;
	signal(SIGTERM, signal_handler);  // kill (-15)
	signal(SIGINT, signal_handler);   // ctrl+C
	signal(SIGQUIT, signal_handler);  // ctrl+\  .
	signal(SIGTSTP, SIG_IGN);         // ctrl+Z
	signal(SIGSEGV, signal_handler);
	printf("my PID: %zd\n", getpid());
	while(1){
		childpid = create_child();
		if(childpid){
			printf("child #%d with PID %d\n", ++i, childpid);
			wait(NULL);
			printf("died\n");
		}else return 0;
	}
	return 0;
}
