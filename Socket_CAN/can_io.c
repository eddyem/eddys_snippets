/* CAN I/O library (to use as a process)
 * usage:
 *   first: fork() + start_can_io(NULL) - start CAN Rx-buffering process
 *   then:  fork() + Control_1(....) - start process that uses recv/send functions
 *     ...........................
 *   then:  fork() + Control_N(....)
 *
 * note: use init_can_io() at the begining of every Control process
 *       BUT DON't USE it in main() before Control process start
 *       ^^^^^^^^^^^^^
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <time.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/shm.h>

#include "can4linux.h"
#include "can_io.h"

char can_dev[40] = "/dev/can0";
int can_fd=-1;
char can_lck[40] = "/tmp/dev_can0.lock";
int can_lk=-1;
static int server_mode=0;
static int my_uid;

#define CAN_SHM_SIZE ((sizeof(int)*3)+CAN_CTLR_SIZE+(CAN_RX_SIZE*sizeof(canmsg_t)))

union ShMkey {
      char  name[5];
      key_t code;
} can_shm_key;
int can_shm_id=-1;
char *can_shm_addr = NULL;
#define can_pid      (*(((int *)can_shm_addr)+0)) /* PID of CAN I/O process */
#define can_open     (*(((int *)can_shm_addr)+1)) /* file descr.of CAN-driver */
#define rx_buff_pntr (*(((int *)can_shm_addr)+2)) /* from 0 till CAN_RX_SIZE-1 */
void *can_ctrl_addr = NULL;  /* shm area reserved for control process purpose*/
canmsg_t *rx_buff;           /* rx ring buffer: CAN_RX_SIZE*sizeof(canmsg_t)*/

struct CMD_Queue {  /* описание очереди (канала) команд */
    union {
      char  name[5];          /* ключ идентефикации очереди */
      key_t code;
    } key;
    int mode;                 /* режим доступа (rwxrwxrwx) */
    int side;                 /* тип подсоединения: Клиент/Сервер (Sender/Receiver)*/
    int id;                   /* дескриптор подсоединения */
    unsigned int acckey;      /* ключ доступа (для передачи Клиент->Сервер) */
};
/* канал команд используем для передачи CAN-фреймов */
static struct CMD_Queue canout = {{{'C','A','N',0,0}},0200,0,-1,0};

/* структура сообщения */
struct my_msgbuf {
   long mtype;            /* type of message */
   unsigned long acckey;  /* ключ доступа клиента */
   unsigned long src_pid; /* номер процесса источника */
   unsigned long src_ip;  /* IP-адр. источника, =0 - локальная команда */
   char mtext[100];       /* message text */
};

static void can_abort(int sig);

void set_server_mode(int mode) {server_mode=mode;}
int can_server() {return(server_mode);}
int can_card() {return(can_fd>0);}
int can_gate() {return(0);}
double can_gate_time_offset() {return(0.0);}

unsigned long get_acckey() {return(0);}

static int shm_created=0;

/* to use _AFTER_  process forking */
void *init_can_io() { /* returns shared area addr. for client control process*/
   int i;
   int new_shm=0;
   char *p, msg[100];

   my_uid=geteuid();
   if(can_shm_addr==NULL) {
      if((p=strrchr(can_dev,'/'))!=NULL) {
     memcpy(&can_lck[9], p+1, 4);
     memcpy(can_shm_key.name, p+1, 4);
     can_shm_key.name[4]='\0';
      } else {
         fprintf(stderr,"Wrong CAN device name: %s\n", can_dev);
         return NULL;
      }
      can_shm_id = shmget(can_shm_key.code, CAN_SHM_SIZE, 0644);
      if(can_shm_id<0 && errno==EACCES)
     can_shm_id = shmget(can_shm_key.code, CAN_SHM_SIZE, 0444);
      if(can_shm_id<0 && errno==ENOENT && server_mode) {
     can_shm_id = shmget(can_shm_key.code, CAN_SHM_SIZE, IPC_CREAT|IPC_EXCL|0644);
     new_shm = shm_created = 1;
      }
      if(can_shm_id<0) {
     can_prtime(stderr);
     if(new_shm)
        sprintf(msg,"Can't create shm CAN buffer '%s'",can_shm_key.name);
     else if(server_mode)
        sprintf(msg,"CAN-I/O: Can't find shm segment for CAN buffer '%s'",can_shm_key.name);
     else
        sprintf(msg,"Can't find shm segment for CAN buffer '%s' (maybe no CAN-I/O process?)",can_shm_key.name);
     perror(msg);
     return NULL;
      }
      can_shm_addr = shmat(can_shm_id, NULL, 0);
      if (can_shm_addr == (void*)-1 && errno==EACCES)
     can_shm_addr = shmat(can_shm_id, NULL, SHM_RDONLY);
      if (can_shm_addr == (void*)-1) {
     sprintf(msg,"Can't attach shm CAN buffer '%s'",can_shm_key.name);
     perror(msg);
     shmctl(can_shm_id, IPC_RMID, NULL);
     return NULL;
      }
   }
   can_ctrl_addr = (canmsg_t *)(can_shm_addr+sizeof(int)*3);
   rx_buff = (canmsg_t *)(can_ctrl_addr+CAN_CTLR_SIZE);

   if(can_fd<0 && canout.id<0) {
      if(server_mode) {
     if(( can_fd = open(can_dev, O_RDWR )) < 0 ) {
        sprintf(msg,"CAN-I/O: Error opening CAN device %s", can_dev);
        can_prtime(stderr);
        perror(msg);
        shmctl(can_shm_id, IPC_RMID, NULL);
        return NULL;
     }
      } else {
     if((canout.id = msgget(canout.key.code, canout.mode)) < 0) {
        sprintf(msg,"Error opening CAN output queue '%s'(maybe no CANqueue server process?) ",canout.key.name);
        perror(msg);
     }
      }
   }
   if(can_lk>0) close(can_lk);
   if(( can_lk = open(can_lck, O_RDWR|O_CREAT, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH )) < 0 ) {
      sprintf(msg,"Error opening CAN device lock-file %s", can_lck);
      perror(msg);
      shmctl(can_shm_id, IPC_RMID, NULL);
      close(can_fd);
      return NULL;
   }
   fchmod(can_lk, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
   if(new_shm) {
      struct timeval tmv;
      struct timezone tz;
      gettimeofday(&tmv,&tz);
      if(flock(can_lk, LOCK_EX)<0) perror("locking CAN");
      can_pid = 0;
      can_open = -1;
      rx_buff_pntr = 0;
      for(i=0; i<CAN_RX_SIZE; i++) {
     rx_buff[i].id = 0;
     rx_buff[i].timestamp = tmv;
      }
      if(flock(can_lk, LOCK_UN)<0) perror("unlocking CAN");
   }
   signal(SIGHUP, can_exit);
   signal(SIGINT, can_exit);
   signal(SIGQUIT,can_exit);
   signal(SIGTERM,can_exit);
   return(can_ctrl_addr);
}

/* CAN "Rx to buff" process */
void *start_can_io() {

   set_server_mode(1);
   init_can_io();
   if(can_io_ok()) {
      can_prtime(stderr);
      fprintf(stderr,"CAN I/O process(%d) already running!\n",can_pid);
      sleep(1);
      can_prtime(stderr);
      fprintf(stderr,"New CAN I/O process(%d) exiting...!\n",getpid());
      exit(0);
   }
   if( can_fd < 0 ) {
      can_prtime(stderr);
      fprintf(stderr,"Error opening CAN device %s\n", can_dev);
      shmctl(can_shm_id, IPC_RMID, NULL);
      exit(0);
   }
   can_pid = getpid();
   can_open = can_fd;

   signal(SIGHUP, can_abort);
   signal(SIGINT, can_abort);
   signal(SIGQUIT,can_abort);
   signal(SIGFPE, can_abort);
   signal(SIGPIPE,can_abort);
   signal(SIGSEGV,can_abort);
   signal(SIGALRM, SIG_IGN);
   signal(SIGTERM,can_abort);

   if(shmctl(can_shm_id, SHM_LOCK, NULL) < 0)
      perror("CAN I/O: can't prevents swapping of Rx-buffer area");

   while(1) {
      int n;
      canmsg_t rx;

      if(!can_io_shm_ok()) {can_delay(0.3); continue;}

      n = can_wait(can_fd, 0.3);
      if(n < 0) sleep(1);
      if(n <= 0) continue;

//    do {
     static struct timeval tm = {0,0};
     n = read(can_fd, &rx, sizeof(canmsg_t));
     if(n < 0)
        perror("CAN Rx error");
     else if(n > 0) {
        /* work around the timestamp bug in old driver version */
        while((double)rx.timestamp.tv_sec+(double)rx.timestamp.tv_usec/1e6 < (double)tm.tv_sec+(double)tm.tv_usec/1e6) {
           rx.timestamp.tv_usec += 10000;
           if(rx.timestamp.tv_usec > 1000000) {
          rx.timestamp.tv_sec++;
          rx.timestamp.tv_usec -= 1000000;
           }
        }
        if(flock(can_lk, LOCK_EX)<0) perror("locking CAN");
        rx_buff[rx_buff_pntr] = rx;
        rx_buff_pntr = (rx_buff_pntr + 1) % CAN_RX_SIZE;
        if(flock(can_lk, LOCK_UN)<0) perror("unlocking CAN");
//fprintf(stderr,"%d read(id=%02x,len=%d)\n",rx_buff_pntr,rx.id,rx.length);fflush(stderr);
       /*fprintf(stderr,"reading CAN: 1 frame\n");*/
     } else {
        fprintf(stderr,"reading CAN: nothing\n");fflush(stderr);
     }
//    } while(n>0);
   }
}

/* put CAN-frame to recv-buffer  */
void can_put_buff_frame(double rtime, int id, int length, unsigned char data[]) {
   int i;
   canmsg_t rx;
   int sec = (int)rtime;
   if(!server_mode) return;
   if(length<0) length=0;
   if(length>8) length=8;
   rx.id=id;
   rx.length=length;
   for(i=0; i<length; i++) rx.data[i]=data[i];
   rx.timestamp.tv_sec = sec;
   rx.timestamp.tv_usec = (int)((rtime-sec)*1000000.);
   if(flock(can_lk, LOCK_EX)<0) perror("locking CAN");
   rx_buff[rx_buff_pntr] = rx;
   rx_buff_pntr = (rx_buff_pntr + 1) % CAN_RX_SIZE;
   if(flock(can_lk, LOCK_UN)<0) perror("unlocking CAN");
}

/* Все нормально с SHM-буфером CAN-I/O процесса */
int can_io_shm_ok() {
   return(can_pid>0 && can_open>0);
}


/* Все нормально с CAN-I/O процессом */
/*  (но надо быть супер-юзером!)    */
int can_io_ok() {
   return(can_io_shm_ok() && (my_uid!=0||kill(can_pid, 0)==0));
}


/* Возможна работа c CAN для клиента */
int can_ok() {
   return(can_io_shm_ok());
}


/* wait for CAN-frame  */
int can_wait(int fd, double tout)
{
  int nfd,width;
  struct timeval tv;
  fd_set readfds;

   if(fd==0 && tout>=0.01) {
      double dt = can_dsleep(tout);
      if(dt>0.) can_dsleep(dt);
      return(0);
   }
   if(fd<0) fd=can_fd;
   if(fd>0) {
      FD_ZERO(&readfds);
      FD_SET(fd, &readfds);
      width = fd+1;
   } else
      width = 0;
   tv.tv_sec = (int)tout;
   tv.tv_usec = (int)((tout - tv.tv_sec)*1000000.+0.9);
slipping:
   if(fd>0 && can_fd>0)
      nfd = select(width, &readfds, (fd_set *)NULL, (fd_set *)NULL,  &tv);
   else
      nfd = select(0, (fd_set *)NULL, (fd_set *)NULL, (fd_set *)NULL,  &tv);
   if(nfd < 0) {
      if(errno == EINTR)
     goto slipping;
      perror("Error in can_wait(){ select() }");
      return(-1);
   } else if(nfd == 0)                 /* timeout! */
      return(0);
   if(fd>0 && FD_ISSET(fd, &readfds))  /* Rx frame! */
      return(1);
   return(0);
}

/* cleanup recv-buffer in client process */
void can_clean_recv(int *pbuf, double *rtime) {
    struct timeval tmv;
    struct timezone tz;
    gettimeofday(&tmv,&tz);
    *pbuf = rx_buff_pntr;
    *rtime = tmv.tv_sec + (double)tmv.tv_usec/1000000.;
}

/* find next rx-frame in recv-buffer for client process */
int can_recv_frame(int *pbuf, double *rtime,
          int *id, int *length, unsigned char data[]) {
   return(can_get_buff_frame(pbuf, rtime, id, length, data));
}
int can_get_buff_frame(int *pbuf, double *rtime,
          int *id, int *length, unsigned char data[]) {
    while(*pbuf != rx_buff_pntr) {
       canmsg_t *rx = &rx_buff[*pbuf];
       struct timeval *tv = &rx->timestamp;
       double t_rx;

       if(flock(can_lk, LOCK_EX)<0) perror("locking CAN");

       t_rx = tv->tv_sec + (double)tv->tv_usec/1000000.;
       if(t_rx+1. >= *rtime) {
      int i;
      *id = rx->id;
      *length = rx->length;
      for(i = 0; i < *length; i++)
         data[i] = rx->data[i];
      *rtime = t_rx;
      *pbuf = (*pbuf + 1) % CAN_RX_SIZE;

      if(flock(can_lk, LOCK_UN)<0) perror("unlocking CAN");
      return(1);
       }
       *pbuf = (*pbuf + 1) % CAN_RX_SIZE;

       if(flock(can_lk, LOCK_UN)<0) perror("unlocking CAN");
    }
    return(0);
}

/* send tx-frame from client process */
/*  to CAN-driver or to output queue */
int can_send_frame(int id, int length, unsigned char data[]) {
    int i, ret=1;
    if(can_fd<0 && canout.id<0)
       return(0);
    if(length>8) length=8;
    if(length<0) length=0;
    if(can_fd>=0) {
       canmsg_t tx;
       tx.id=id;
       tx.cob=0;
       tx.flags=0;
       tx.length=length;
       for(i=0;i<length;i++) tx.data[i]=data[i];
       if(flock(can_lk, LOCK_EX)<0) perror("locking CAN");
       ret = write(can_fd, &tx, sizeof(canmsg_t));
       if(flock(can_lk, LOCK_UN)<0) perror("unlocking CAN");
       if(server_mode)
      /* copy tx CAN-frame back to recv-buffer  */
      can_put_buff_frame(can_dtime(), id, length, data);
    } else if(canout.id>=0) {
       struct my_msgbuf mbuf;
       mbuf.src_pid = getpid();
       mbuf.src_ip = 0;
       mbuf.acckey = canout.acckey;
       mbuf.mtype = id;
       for(i=0;i<length;i++) mbuf.mtext[i]=data[i];
       msgsnd( canout.id, (struct msgbuf *)&mbuf, length+12, IPC_NOWAIT);
    }
    return(ret);
}

static void can_abort(int sig) {
    char ss[10];
    struct shmid_ds buf;

    if(sig) signal(sig,SIG_IGN);
    if(!server_mode) can_exit(sig);
    switch (sig) {
    case 0      : strcpy(ss," ");  break;
    case SIGHUP : strcpy(ss,"SIGHUP -");  break;
    case SIGINT : strcpy(ss,"SIGINT -");  break;
    case SIGQUIT: strcpy(ss,"SIGQUIT -"); break;
    case SIGFPE : strcpy(ss,"SIGFPE -");  break;
    case SIGPIPE: strcpy(ss,"SIGPIPE -"); break;
    case SIGSEGV: strcpy(ss,"SIGSEGV -"); break;
    case SIGTERM: strcpy(ss,"SIGTERM -"); break;
    default:  sprintf(ss,"SIG_%d -",sig); break;
    }
    switch (sig) {
    default:
    case SIGHUP :
    case SIGINT :
         can_prtime(stderr);
         fprintf(stderr,"CAN I/O: %s Ignore .....\n",ss);
         fflush(stderr);
         signal(sig, can_abort);
         return;
    case SIGPIPE:
    case SIGQUIT:
    case SIGFPE :
    case SIGSEGV:
    case SIGTERM:
         signal(SIGALRM, can_abort);
         alarm(2);
         can_prtime(stderr);
         fprintf(stderr,"CAN I/O: %s process should stop after 2sec delay...\n",ss);
         fflush(stderr);
         close(can_fd);
         can_fd = can_open = -1;
         return;
    case SIGALRM:
         can_prtime(stderr);
         fprintf(stderr,"CAN I/O: process stop!\n");
         fflush(stderr);
         close(can_lk);
         can_lk = -1;
         can_pid = 0;
         shmdt(can_shm_addr);
         shmctl(can_shm_id, IPC_STAT, &buf);
         if(buf.shm_nattch==0) {
            shmctl(can_shm_id, SHM_UNLOCK, NULL);
            shmctl(can_shm_id, IPC_RMID, NULL);
         }
         exit(sig);
    }
}

void can_exit(int sig) {
    char ss[12];

    struct shmid_ds buf;
    if(sig) signal(sig,SIG_IGN);
    if(server_mode) can_abort(sig);
    switch (sig) {
    case 0      : strcpy(ss,"Exiting - ");  break;
    case SIGHUP : strcpy(ss,"SIGHUP -");  break;
    case SIGINT : strcpy(ss,"SIGINT -");  break;
    case SIGQUIT: strcpy(ss,"SIGQUIT -"); break;
    case SIGFPE : strcpy(ss,"SIGFPE -");  break;
    case SIGPIPE: strcpy(ss,"SIGPIPE -"); break;
    case SIGSEGV: strcpy(ss,"SIGSEGV -"); break;
    case SIGTERM: strcpy(ss,"SIGTERM -"); break;
    default:  sprintf(ss,"SIG_%d -",sig); break;
    }
    switch (sig) {
    default:
    case SIGHUP :
         can_prtime(stderr);
         fprintf(stderr,"%s Ignore .....\n",ss);
         fflush(stderr);
         signal(sig, can_exit);
         return;
    case 0:
    case SIGINT :
    case SIGPIPE:
    case SIGQUIT:
    case SIGFPE :
    case SIGSEGV:
    case SIGTERM:
         if(can_fd>=0) close(can_fd);
         can_prtime(stderr);
         fprintf(stderr,"%s process stop!\n",ss);
         fflush(stderr);
         close(can_lk);
         shmdt(can_shm_addr);
         shmctl(can_shm_id, IPC_STAT, &buf);
         if(buf.shm_nattch==0)
            shmctl(can_shm_id, IPC_RMID, NULL);
         exit(sig);
    }
}

char *time2asc(double t)
{
 static char stmp[10][20];
 static int itmp=0;
    char *lin = stmp[itmp];
    int h, min;
    double sec;
    h   = (int)(t/3600.);
    min = (int)((t - (double)h*3600.)/60.);
    sec = t - (double)h*3600. - (double)min*60.;
    h %= 24;
    sprintf(lin, "%02d:%02d:%09.6f", h,min,sec);
    itmp = (itmp+1)%10;
    return lin;
}

double can_dsleep(double dt) {
   struct timespec ts,tsr;
   ts.tv_sec = (time_t)dt;
   ts.tv_nsec = (long)((dt-ts.tv_sec)*1e9);
   nanosleep(&ts,&tsr);
   return((double)ts.tv_sec + (double)ts.tv_nsec/1e9);
}

double can_dtime() {
   struct timeval ct;
   struct timezone tz;
   gettimeofday(&ct, &tz);
   return ((double)ct.tv_sec + (double)ct.tv_usec/1e6);
}

char *can_atime() {return(time2asc(can_dtime()));}

void can_prtime(FILE *fd) {
   static double otime=0.0;
   double ntime=can_dtime();
   time_t itime = (int)ntime;
   if(otime==0.0) tzset();
   ntime -= (double)timezone;
   if((((int)ntime)%(24*3600) < ((int)otime)%(24*3600)) || otime==0.0)
      fprintf(fd,"========================\n%s",ctime(&itime));
   fprintf(fd,"%s ",time2asc(ntime));
   otime=ntime;
}
