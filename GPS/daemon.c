/*
 * daemon.c - functions for running in background like a daemon
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

#include "daemon.h"
#include <stdio.h>		// printf, fopen, ...
#include <unistd.h>		// getpid
#include <stdio.h>		// perror
#include <sys/types.h>	// opendir
#include <dirent.h>		// opendir
#include <sys/stat.h>	// stat
#include <fcntl.h>		// fcntl
#include <stdlib.h>		// exit
#include <string.h>		// memset

/**
 * read process name from /proc/PID/cmdline
 * @param pid - PID of interesting process
 * @return filename or NULL if not found
 * 		don't use this function twice for different names without copying
 * 		its returning by strdup, because `name` contains in static array
 */
char *readname(pid_t pid){
	static char name[256];
	char *pp = name, byte, path[256];
	FILE *file;
	int cntr = 0;
	size_t sz;
	snprintf (path, 255, PROC_BASE "/%d/cmdline", pid);
	file = fopen(path, "r");
	if(!file) return NULL; // there's no such file
	do{	// read basename
		sz = fread(&byte, 1, 1, file);
		if(sz != 1) break;
		if(byte != '/') *pp++ = byte;
		else{
			pp = name;
			cntr = 0;
		}
	}while(byte && cntr++ < 255);
	name[cntr] = 0;
	fclose(file);
	return name;
}

void iffound_default(pid_t pid){
	fprintf(stderr, "\nFound running process (pid=%d), exit.\n", pid);
	exit(0);
}

/**
 * check wether there is a same running process
 * exit if there is a running process or error
 * Checking have 3 steps:
 * 		1) lock executable file
 * 		2) check pidfile (if you run a copy?)
 * 		3) check /proc for executables with the same name (no/wrong pidfile)
 * @param argv - argument of main() or NULL for non-locking, call this function before getopt()
 * @param pidfilename - name of pidfile or NULL if none
 * @param iffound - action to run if file found or NULL for exit(0)
 */
void check4running(char **argv, char *pidfilename, void (*iffound)(pid_t pid)){
	DIR *dir;
	FILE *pidfile, *fself;
	struct dirent *de;
	struct stat s_buf;
	pid_t pid = 0, self;
	struct flock fl;
	char *name, *myname;
	if(!iffound) iffound = iffound_default;
	if(argv){ // block self
		fself = fopen(argv[0], "r"); // open self binary to lock
		memset(&fl, 0, sizeof(struct flock));
		fl.l_type = F_WRLCK;
		if(fcntl(fileno(fself), F_GETLK, &fl) == -1){ // check locking
			perror("fcntl");
			exit(1);
		}
		if(fl.l_type != F_UNLCK){ // file is locking - exit
			printf("Found locker, PID = %d!\n", fl.l_pid);
			exit(1);
		}
		fl.l_type = F_RDLCK;
		if(fcntl(fileno(fself), F_SETLKW, &fl) == -1){
			perror("fcntl");
			exit(1);
		}
	}
	self = getpid(); // get self PID
	if(!(dir = opendir(PROC_BASE))){ // open /proc directory
		perror(PROC_BASE);
		exit(1);
	}
	if(!(name = readname(self))){ // error reading self name
		perror("Can't read self name");
		exit(1);
	}
	myname = strdup(name);
	if(pidfilename && stat(pidfilename, &s_buf) == 0){ // pidfile exists
		pidfile = fopen(pidfilename, "r");
		if(pidfile){
			if(fscanf(pidfile, "%d", &pid) > 0){ // read PID of (possibly) running process
				if((name = readname(pid)) && strncmp(name, myname, 255) == 0)
					iffound(pid);
			}
			fclose(pidfile);
		}
	}
	// There is no pidfile or it consists a wrong record
	while((de = readdir(dir))){ // scan /proc
		if(!(pid = (pid_t)atoi(de->d_name)) || pid == self) // pass non-PID files and self
			continue;
		if((name = readname(pid)) && strncmp(name, myname, 255) == 0)
			iffound(pid);
	}
	closedir(dir);
	if(pidfilename){
		pidfile = fopen(pidfilename, "w");
		fprintf(pidfile, "%d\n", self); // write self PID to pidfile
		fclose(pidfile);
	}
	free(myname);
}
