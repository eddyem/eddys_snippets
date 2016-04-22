#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>

void xor(char *buf, char *pattern, size_t l){
	size_t i;
	for(i = 0; i < l; ++i)
		printf("%c", buf[i] ^ pattern[i]);
}

int main(int argc, char **argv){
	if(argc != 3){
		printf("Usage: %s <infile> <password>\n", argv[0]);
		return -1;
	}
	char *key = strdup(argv[2]);
	size_t n = 0, keylen = strlen(key);
	char *buff = malloc(keylen);
	int f = open(argv[1], O_RDONLY);
	if(f < 0){
		perror("Can't open file");
		return f;
	}
	do{
		n = read(f, buff, keylen);
		xor(buff, key, n);
	}while(n);
	return 0;
}
