#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <err.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

#define BUFSZ 1024

static void getpath(const char *modname, char *buf, size_t bufsize){
	int filedes[2];
	char cmd[256];
	ssize_t count;
	snprintf(cmd, 255, "modinfo -F filename %s", modname);
	if(pipe(filedes) == -1)
		err(3, "pipe");
	pid_t pid = fork();
	if(pid == -1)
		err(4, "fork");
	else if (pid == 0){
		while ((dup2(filedes[1], STDOUT_FILENO) == -1) && (errno == EINTR)) {}
		close(filedes[1]);
		close(filedes[0]);
		system(cmd);
		exit(0);
	}
	while(1){
		count = read(filedes[0], buf, bufsize);
		if(count == -1){
			if(errno == EINTR)
				continue;
			else
				err(7, "read");
		}else
			break;
	}
	if(count > 1) buf[count-1] = 0;
	close(filedes[0]);
	wait(0);
}

int main(){
	int fd = open("/proc/modules", O_RDONLY);
	if(fd < 0) err(1, "open");
	char buf[BUFSZ+1], *end = buf;
	size_t readed, toread = BUFSZ;
	buf[BUFSZ] = 0;
	while(1){
		readed = read(fd, end, toread);
		if(readed < 1 && end == buf) break;
		end += readed;
		*end = 0;
		char *E;
#define getspace(X)  {E = strchr(X, ' '); if(!E) break; *E = 0; E++;}
		getspace(buf);
		char *name = buf, *start = E;
		if(start >= end) break;
		getspace(start);
#undef getspace
		size_t size = atoll(start);
		char path[1024];
		getpath(name, path, sizeof(path));
		printf("%s: %zd\n", path, size);
		if(E < end){
			if((start = strchr(E, '\n'))) start++;
			else start = end;
		}else start = E;
		if(start < end){
			readed = end - start;
			memmove(buf, start, readed);
			end = buf + readed;
			toread = BUFSZ - readed;
		}else{
			toread = BUFSZ;
			end = buf;
		}
	}
	close(fd);
	return 0;
}
