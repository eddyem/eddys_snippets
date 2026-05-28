#include <stdio.h>
#include <unistd.h>

static unsigned long long get_available_mem(){
    return sysconf(_SC_AVPHYS_PAGES) * (unsigned long long) sysconf(_SC_PAGE_SIZE);
}

int main(){
	unsigned long long m = get_available_mem();
	printf("MEM: %llu == %lluGB\n", m, m/1024/1024/1024);
	return 0;
}

// Never allocate memory by big pieces with malloc! Only mmap with MAP_POPULATE!!!!!!!!!11111111111
