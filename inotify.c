#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/inotify.h>
#include <unistd.h>

/* Read all available inotify events from the file descriptor 'fd'.
  wd is the table of watch descriptors for the directories in argv.
  argc is the length of wd and argv.
  argv is the list of watched directories.
  Entry 0 of wd and argv is unused. */

static void
handle_events(int fd, int wd, int argc, char* argv[])
{
   /* Some systems cannot read integer variables if they are not
      properly aligned. On other systems, incorrect alignment may
      decrease performance. Hence, the buffer used for reading from
      the inotify file descriptor should have the same alignment as
      struct inotify_event. */
    struct inotify_event buf;
   int i;
   ssize_t len;
   char *ptr;

   /* Loop while events can be read from inotify file descriptor. */

   for (;;) {

       /* Read some events. */

       len = read(fd, &buf, sizeof buf);
       if (len == -1 && errno != EAGAIN) {
           perror("read");
           exit(EXIT_FAILURE);
       }

       /* If the nonblocking read() found no events to read, then
          it returns -1 with errno set to EAGAIN. In that case,
          we exit the loop. */

       if (len <= 0)
           break;
       if (buf.mask & IN_CLOSE_WRITE)
           printf("IN_CLOSE_WRITE: ");
       /* Print the name of the watched directory */
       for (i = 1; i < argc; ++i) {
           if (wd == buf.wd) {
               printf("%s/", argv[i]);
               break;
           }
       }

       /* Print the name of the file */

       if (buf.len)
           printf("%s", buf.name);

       /* Print type of filesystem object */

       if (buf.mask & IN_ISDIR)
           printf(" [directory]\n");
       else
           printf(" [file]\n");
   }

}

int
main(int argc, char* argv[])
{
    int fd, i, poll_num;
    int wd;
    struct pollfd fds;

    if (argc != 2) {
       printf("Usage: %s file\n", argv[0]);
       exit(EXIT_FAILURE);
    }
    fd = inotify_init1(IN_NONBLOCK);
    if(fd == -1) {
       perror("inotify_init1");
       exit(EXIT_FAILURE);
    }
    // check if there's a file
    FILE* f = fopen(argv[1], "r");
    if(!f){perror("open"); return 1;}
    char buf[256];
    if(fread(buf, 1, 256, f) < 1){ perror("read"); return 1;}
    fclose(f);

    if(-1 == (wd = inotify_add_watch(fd, argv[1], IN_CLOSE_WRITE))){
        perror("inotify_add_watch");
        exit(EXIT_FAILURE);
    }
    /* Inotify input */
    fds.fd = fd;
    fds.events = POLLIN;
    /* Wait for events */
    printf("Listening for events.\n");
    while(1){
       poll_num = poll(&fds, 1, -1);
       if(poll_num == -1){
           if (errno == EINTR)
               continue;
           perror("poll");
           exit(EXIT_FAILURE);
       }
       if (poll_num > 0) {
           if(fds.revents & POLLIN){
               handle_events(fd, wd, argc, argv);
           }
       }
    }
    close(fd);
    exit(EXIT_SUCCESS);
}
