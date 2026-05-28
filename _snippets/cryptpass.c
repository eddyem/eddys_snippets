#define _GNU_SOURCE
#include <crypt.h> // gcc -lcrypt
#include <errno.h>
#include <stdio.h>

int main(int argc, char **argv){
    if(argc != 3){
        printf("USAGE:\n\t%s <password> <salt>\n", argv[0]);
        printf("Example:\n\t%s password \\$6\\$salt  -> SHA512 with given salt (see man 3 crypt)\n", argv[0]);
        return 1;
    }
    struct crypt_data x;
    x.initialized = 0;

    char *c = crypt_r(argv[1], argv[2], &x);
    int e = errno;
    if(!c){
        switch(e){
            case EINVAL:
                fprintf(stderr, "Wrong SALT format!\n");
            break;
            case ENOSYS:
                fprintf(stderr, "crypt() not realized!\n");
            break;
            case EPERM:
                fprintf(stderr, "/proc/sys/crypto/fips_enabled prohibits this encryption method!\n");
            default:
                fprintf(stderr, "Unknown error!\n");
        }
        return e;
    }
    printf("Result of crypt_r():\n%s\n", c);
    return 0;
}
