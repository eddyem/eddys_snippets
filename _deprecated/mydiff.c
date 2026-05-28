#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <err.h>

void help(char* name){
	printf("\nUsage:\n\t%s <file1> <file2>\n\t\tprint lines of <file1>, that not present in <file2>\n", name);
	printf("\t%s -v <file1> < file2>\n\t\tshows lines of <file1>, that present in <file2>\n", name);
	exit(0);
}

int main(int argc, char** argv){
	char *buf, *F1, *F2, *ptr, *ptr1;
	int file1, file2, n = 0, Vflag = 0;
	long size1, size2;
	struct stat St;
	if(argc < 3 || strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help")  == 0) help(argv[0]);
	if(strcmp(argv[1], "-v") == 0){ Vflag = 1; n = 1;}
	if( stat(argv[n+1], &St) < 0) err(1, "\n\tCan't stat %s", argv[n+1]);
	size1 = St.st_size;
	if( stat(argv[n+2], &St) < 0) err(2, "\n\tCan't stat %s", argv[n+2]);
	size2 = St.st_size;
	file1 = open(argv[n+1], O_RDONLY);
	if(file1 < 0) err(3, "\n\tCan't open %s", argv[n+1]);
	file2 = open(argv[n+2], O_RDONLY);
	if(file2 < 0) err(4, "\n\tCan't open %s", argv[n+2]);
	buf = malloc(16385); // буфер для строки
	F1 = malloc(size1 + 1); // содержимое файла 1
	ptr1 = F1;
	F2 = malloc(size2 + 1); // содержимое файла 2
	if(read(file1, F1, size1) != size1) err(5, "\n\tCan't read %s", argv[n+1]);
	F1[size1] = 0;
	close(file1);
	if(read(file2, F2, size2) != size2) err(6, "\n\tCan't read %s", argv[n+2]);
	F2[size2] = 0;
	close(file2);
	while(ptr1){
		ptr = strchr(ptr1, '\n');
		if(ptr) *ptr = 0;
		strncpy(buf, ptr1, 16384);
		if(strstr(F2, buf) == NULL){ if (Vflag == 0) printf("%s\n", buf);}
		else if(Vflag == 1) printf("%s\n", buf);
		if(ptr) ptr1 = ptr + 1;
		else ptr1 = NULL; // конец строки
	}
	free(buf); free(F1); free(F2);
	exit(0);
}
