/*
 * This file is part of the SocketCAN project.
 * Copyright 2021 Edward V. Emelianov <edward.emelianoff@gmail.com>.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


int readCAN(int sock, struct can_frame *frame){
    fd_set fds;
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 1000; // wait not more than 1millisecond
    FD_ZERO(&fds);
    FD_SET(sock, &fds);
    do{
        int rc = select(sock+1, &fds, NULL, NULL, &timeout);
        if(rc < 0){
            if(errno != EINTR){
                WARN("select()");
                return 0;
            }
            continue;
        }
        break;
    }while(1);
    if(FD_ISSET(sock, &fds)){
        return read(sock, frame, sizeof(struct can_frame));
    }
    return 0;
}



int sock = socket(PF_CAN, SOCK_RAW, CAN_RAW);
if(sock < 0) ERR("Can't create socket");
struct sockaddr_can addr;
struct ifreq ifr;
strcpy(ifr.ifr_name, "can0");
if(ioctl(sock, SIOCGIFINDEX, &ifr) < 0) ERR("ioctl()");
addr.can_family = AF_CAN;
addr.can_ifindex = ifr.ifr_ifindex;
if(bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) ERR("bind()");
int nbytes = 0;
struct can_frame frame = {0};
double dt = dtime();
do{
    nbytes = readCAN(sock, &frame);
    if(nbytes < 0) ERR("can raw socket read");
}while(nbytes == 0 && dtime() - dt < 10.);
printf("got frame len=%d id=%d", frame.can_dlc, frame.can_id);
if(frame.can_dlc) printf(", data: ");
for(int i = 0; i < frame.can_dlc; ++i)
    printf("0x%02x ", frame.data[i]);
printf("\n");
frame.can_dlc = 6;
frame.can_id = 0xaa;
const uint8_t d[] = {1, 2, 3, 4, 5, 6, 0, 0};
memcpy(&frame.data, d, 8);
int n = write(sock, &frame, sizeof(struct can_frame));
if(sizeof(struct can_frame) != n){
    printf("n=%d\n", n);
    WARN("write()");
}
close(sock);
