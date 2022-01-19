#define CAN_CTLR_SIZE 300      /* size of client process shared area */
#define CAN_RX_SIZE  1000      /* max. # frames in Rx-buffer */

int can_wait(int fd, double tout);
#define can_delay(Tout) can_wait(0, Tout)
void set_server_mode(int mode);
int can_server();
int can_card();
int can_gate();
double can_gate_time_offset();
unsigned long get_acckey();
void *init_can_io();
void *start_can_io();
void can_put_buff_frame(double rtime, int id, int length, unsigned char data[]);
int can_io_ok();
int can_io_shm_ok();
int can_ok();
void can_clean_recv(int *pbuf, double *rtime);
int can_get_buff_frame(int *pbuf, double *rtime,
          int *id, int *length, unsigned char data[]);
int can_recv_frame(int *pbuf, double *rtime,
          int *id, int *length, unsigned char data[]);
int can_send_frame(int id, int length, unsigned char data[]);
void can_exit(int sig);
char *time2asc(double t);
double can_dsleep(double dt);
double can_dtime();
char *can_atime();
void can_prtime(FILE *fd);
