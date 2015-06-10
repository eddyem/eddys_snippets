#include <sys/stat.h>
#include <dirent.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <mntent.h>

#define ROOTINO     (2)
#define BUFSZ       (128)

typedef struct _mntdef{
	char *mntpoint;
	struct _mntdef *next;
} mntdef;

mntdef *mntlist = NULL;

void addmntdef(char *name){
	mntdef *m, *last = NULL;
	last = mntlist;
	if(last){
		do{
			if(strcmp(last->mntpoint, name) == 0) return;
			if(last->next) last = last->next;
			else break;
		}while(1);
	}
	m = calloc(1, sizeof(mntdef));
	m->mntpoint = strdup(name);
	if(!last) mntlist = m;
	else last->next = m;
}


void push_dir(char **old, char* new, size_t *len, size_t *L){
	size_t dlen = strlen(new) + 2;
	size_t plen = strlen(*old);
	if(plen + dlen < *L){
		*old = realloc(*old, plen + dlen);
		*L = plen + dlen;
	}
	if(len)
		memmove(*old + dlen - 1, *old, plen+1);
	memcpy(*old + 1, new, dlen-2);
	if(plen == 0) *(*old + dlen - 1) = 0;
	**old = '/';
}

struct stat original_st;
char *chkmounted(char *path){
	mntdef *m = mntlist;
	char p[1024];
	struct stat st;
	do{
		snprintf(p, 1023, "%s%s", m->mntpoint, path);
		stat(p, &st);
		if(st.st_ino == original_st.st_ino){
			if(strcmp("/", m->mntpoint))
				return m->mntpoint + 1;
			else
				return NULL;
		}
		if(m->next) m = m->next;
		else break;
	}while(1);
	return NULL;
}

char *pwd(char **path, size_t *len){
	size_t L = 0;
	struct dirent *dp;
	struct stat st;
	stat(".", &st);
	if(!path) return NULL;
	if(!*path){
		*path = malloc(BUFSZ + 1);
		**path = 0;
		L = BUFSZ;
		if(len) *len = BUFSZ;
	}else{
		if(len) L = *len;
		else return NULL;
	}
	if(st.st_ino == ROOTINO){
		char *m = NULL;
		if((m = chkmounted(*path))) push_dir(path, m, len, &L);
		if(**path != '/'){
			**path = '/';
			*(*path+1) = 0;
		}
		return *path;
	}
	DIR *dirp = opendir("..");
	while((dp = readdir(dirp))){
			if(strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0)
				continue;
			if(st.st_ino == dp->d_ino){
					push_dir(path, dp->d_name, len, &L);
					chdir("..");
					return pwd(path, &L);
			}
	}
	chdir("..");
	return pwd(path, &L);
	closedir(dirp);
	return *path;
}


int main(int argc, char **argv){
	char *path = NULL;
	struct stat st;
	struct mntent *m;
	FILE *f;
	f = setmntent(_PATH_MOUNTED, "r");
	while((m = getmntent(f))){
		stat(m->mnt_dir, &st);
		addmntdef(m->mnt_dir);
	}
	endmntent(f);
	if(argc == 2){
		if(chdir(argv[1])){
			perror("chdir()");
			return -1;
		}
	}
	stat(".", &original_st);
	puts(pwd(&path, NULL));
	return 0;
}
