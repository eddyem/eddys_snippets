#include <unistd.h>
#include <sys/sendfile.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <sys/mman.h>

/*
./sendfile Titanik.mp4 
Copied by sendfile, time: 30.1055s
Copied by mmap, time: 35.6469s

du Titanik.mp4 
2.6GTitanik.mp4
*/


double dtime(){
	double t;
	struct timeval tv;
	gettimeofday(&tv, NULL);
	t = tv.tv_sec + ((double)tv.tv_usec)/1e6;
	return t;
}

typedef struct{
	char *data;
	size_t len;
} mmapbuf;

void My_munmap(mmapbuf **b){
	if(munmap((*b)->data, (*b)->len))
		perror("Can't munmap");
	free(*b);
	*b = NULL;
}

mmapbuf *My_mmap(char *filename){
	int fd;
	char *ptr = NULL;
	size_t Mlen;
	struct stat statbuf;
	if((fd = open(filename, O_RDONLY)) < 0){
		perror("Can't open file for reading");
		goto ret;
	}
	if(fstat (fd, &statbuf) < 0){
		perror("Can't stat file");
		goto ret;
	}
	Mlen = statbuf.st_size;
	if((ptr = mmap (0, Mlen, PROT_READ, MAP_PRIVATE, fd, 0)) == MAP_FAILED){
		perror("Mmap error for input");
		goto ret;
	}
	mmapbuf *ret = calloc(sizeof(mmapbuf), 1);
	if(ret){
		ret->data = ptr;
		ret->len = Mlen;
	}else munmap(ptr, Mlen);
ret:
	if(close(fd)) perror("Can't close mmap'ed file");
	return  ret;
}


int main(int argc, char **argv){
	struct stat st;
	double T0;
	if(argc != 2){
		printf("Usage: %s file\n", argv[0]);
		return 2;
	}
	if (stat(argv[1], &st) == 0 && S_ISREG(st.st_mode)){
		off_t off = 0;
		int fd = open(argv[1], O_RDONLY);
		int fdo = open("tmpoutput", O_WRONLY | O_CREAT,  S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
		if (fd < 0 || fdo < 0){
			perror("can't open");
			return 1;
		}
		T0 = dtime();
		posix_fadvise(fd, 0, 0, POSIX_FADV_SEQUENTIAL);
		while (off < st.st_size) {
			ssize_t bytes = sendfile(fdo, fd, &off, st.st_size - off);
			if (bytes <= 0){
				perror("can't sendfile");
			}
		}
		close(fd);
		close(fdo);
		printf("Copied by sendfile, time: %gs\n", dtime()-T0);
		T0 = dtime();
		mmapbuf *map = My_mmap(argv[1]);
		if(map){
			fdo = open("tmpoutput", O_WRONLY | O_CREAT,  S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
			if (fd < 0 || fdo < 0){
				perror("can't open");
				return 1;
			}
			size_t written = 0, towrite = map->len;
			char *ptr = map->data;
			do{
				ssize_t wr = write(fdo, ptr, towrite);
				if(wr <= 0) break;
				written += wr;
				towrite -= wr;
				ptr += wr;
			}while(towrite);
			if(written != map->len){
				printf("err: writed only %zd byted of %zd\n", written, map->len);
			}
			close(fdo);
			My_munmap(&map);
			printf("Copied by mmap, time: %gs\n", dtime()-T0);
		}
	}else
		perror("Can't stat file");
	return 0;
}
