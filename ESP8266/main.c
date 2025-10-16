/*
 * This file is part of the esp8266 project.
 * Copyright 2025 Edward V. Emelianov <edward.emelianoff@gmail.com>.
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

#include <stdio.h>
#include <string.h>
#include <usefull_macros.h>

#include "esp8266.h"
#include "serial.h"

static struct{
    int help;
    char *serialdev;
    char *SSID;
    char *SSpass;
    int speed;
    char *port;
    int reset;
    int list;
} G = {
    .speed = 115200,
    .serialdev = "/dev/ttyUSB0",
    .port = "1111",
};

static sl_option_t options[] = {
    {"help",    NO_ARGS,    NULL,   'h',    arg_int,    APTR(&G.help),      "show this help"},
    {"device",  NEED_ARG,   NULL,   'd',    arg_string, APTR(&G.serialdev), "serial device path (default: /dev/ttyUSB0)"},
    {"baudrate",NEED_ARG,   NULL,   'b',    arg_int,    APTR(&G.speed),     "serial speed"},
    {"ssid",    NEED_ARG,   NULL,   0,      arg_string, APTR(&G.SSID),      "SSID to connect"},
    {"pass",    NEED_ARG,   NULL,   0,      arg_string, APTR(&G.SSpass),    "SSID password"},
    {"port",    NEED_ARG,   NULL,   'p',    arg_string, APTR(&G.port),      "servr port (default: 1111)"},
    {"reset",   NO_ARGS,    NULL,   0,      arg_int,    APTR(&G.reset),     "reset ESP"},
    {"list",    NO_ARGS,    NULL,   'l',    arg_int,    APTR(&G.list),      "list available APs"},
    end_option
};

void signals(int signo){
    esp_close();
    serial_close();
    exit(signo);
}

static esp_clientstat_t parse_message(int fd){
    // this isn't a good idea to send data while not received all; so we use buffer
    static sl_ringbuffer_t *rb = NULL;
    if(!rb) rb = sl_RB_new(4096);
    const char *msg = NULL;
    char buf[4096];
    while((msg = esp_msgline())){
        printf("Received line from fd %d: %s\n", fd, msg);
        // do something with this data
        if(0 == strcmp(msg, "help")){
            sl_RB_writestr(rb, "Hey, we don't have any help yet, try `time`\n");
        }else if(0 == strcmp(msg, "time")){
            snprintf(buf, 64, "TIME=%.3f\n", sl_dtime());
            sl_RB_writestr(rb, buf);
        }else{
            if(*msg){
                snprintf(buf, 127, "Part of your message: _%s_\n", msg);
                sl_RB_writestr(rb, buf);
            }
        }
    }
    size_t L = 4094;
    char *b = buf;
    while(sl_RB_readline(rb, b, L) > 0){ // there's a bug in my library! Need to fix (sl_RB_readline returns NOT an amount of bytes)
        size_t got = strlen(b);
        L -= ++got;
        b += got;
        b[-1] = '\n';
    }
    if(L == 4094) return ESP_CLT_OK; // nothing to send
    return esp_send(fd, buf) ? ESP_CLT_OK : ESP_CLT_ERROR;
}

static void processing(){
    uint8_t connected[ESP_MAX_CLT_NUMBER] = {0};
    esp_clientstat_t oldstat[ESP_MAX_CLT_NUMBER] = {0};
    double T0 = sl_dtime();
    int N = -1;
    void chkerr(){
        if(oldstat[N] == ESP_CLT_ERROR){
            DBG("error again -> turn off");
            connected[N] = 0;
        }
    }
    while(1){
        esp_clientstat_t s = esp_process(&N);
        if(N > -1 && N < ESP_MAX_CLT_NUMBER){ // parsing
            //if(s == ESP_CLT_IDLE){ usleep(1000); continue; }
            switch(s){
                case ESP_CLT_CONNECTED:
                    connected[N] = 1;
                    green("Connection on fd=%d\n", N);
                    break;
                case ESP_CLT_DISCONNECTED:
                    connected[N] = 0;
                    green("fd=%d disconnected\n", N);
                    break;
                case ESP_CLT_ERROR:
                    DBG("Error from %d", N);
                    chkerr();
                    break;
                case ESP_CLT_GETMESSAGE:
                    DBG("%d have message", N);
                    s = parse_message(N);
                    if(s == ESP_CLT_ERROR) chkerr();
                    break;
                default: break;
            }
            oldstat[N] = s;
        }
        // and here we can do something for all
        int R = rand() % 5000;
        // example of `broadcast` message
        if(R < 2){
            DBG("Send 'broadcasting' message");
            char buf[64];
            snprintf(buf, 63, "Hello, there's %.2f seconds from start\n", sl_dtime() - T0);
            for(int i = 0; i < ESP_MAX_CLT_NUMBER; ++i){
                if(!connected[i]) continue;
                if(ESP_CLT_ERROR == esp_send(i, buf)) chkerr();
            }
        }
        usleep(1000);
    }
}

int main(int argc, char **argv){
    sl_init();
    sl_parseargs(&argc, &argv, options);
    if(G.help) sl_showhelp(-1, options);
    if(!serial_init(G.serialdev, G.speed)) ERRX("Can't open %s at speed %d", G.serialdev, G.speed);
    if(G.reset){
        red("Resetting, please wait!\n");
        esp_reset();
        usleep(500000);
        DBG("Get buff");
        char *str;
        while((str = serial_getline(NULL, NULL))) if(*str) printf("\t%s\n", str);
        signals(0);
    }
    if(!esp_check()) ERRX("No answer from ESP");
    if(G.list){
        if(esp_listAP()){
            green("Available wifi:\n");
            char *str;
            while(str = serial_getline(NULL, NULL)) printf("\t%s\n", str);
        }else WARNX("Error listing");
    }
    esp_cipstatus_t st = esp_cipstatus();
    if(st != ESP_CIP_GOTIP && st != ESP_CIP_CONNECTED){
        DBG("Need to connect");
        if(!esp_connect(G.SSID, G.SSpass)) ERRX("Can't connect");
    }
    if(esp_myip()){
        green("Connected\n");
        char *str;
        while(str = serial_getline(NULL, NULL)) if(*str) printf("\t%s\n", str);
    }
    ep_stop_server(); // stop just in case
    if(!esp_start_server(G.port)) ERRX("Can't start server");
    processing();
    serial_close();
    return 0;
}
