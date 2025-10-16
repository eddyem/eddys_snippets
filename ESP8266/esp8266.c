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

#include <usefull_macros.h>
#include <stdio.h>

#include "esp8266.h"
#include "serial.h"

#define ESPMSG(x)  do{if(!serial_send_msg(x)) return FALSE;}while(0)

// sometimes ESP hangs with message "WIFI GOT IP" and I can do nothing except waiting

const char *msgptr = NULL; // pointer to received message
static int receivedlen = 0;

// check module working
int esp_check(){
    // try simplest check for three times
    for(int i = 0; i < 3; ++i) if( ANS_OK == serial_sendwans("AT")) break;
    //esp_close();
    // now try next even if no answer for "AT"
    if( ANS_OK == serial_sendwans("ATE0") // echo off
        && ANS_OK == serial_sendwans("AT+CWMODE_CUR=1") // station mode
        ) return TRUE;
    return FALSE;
}

// check established wifi connection
esp_cipstatus_t esp_cipstatus(){
    if(ANS_OK != serial_sendwans("AT+CIPSTATUS")) return ESP_CIP_FAILED;
    char *l = serial_getline(NULL, NULL);
    const char *n = NULL;
    if(!l || !(n = serial_tidx(l, "STATUS:"))) return ESP_CIP_FAILED;
    return (esp_cipstatus_t)serial_s2i(n);
}

// connect to AP
int esp_connect(const char *SSID, const char *pass){
    ESPMSG("AT+CWJAP_CUR=\"");
    ESPMSG(SSID);
    ESPMSG("\",\"");
    ESPMSG(pass);
    if(ANS_OK != serial_sendwans("\"")) return FALSE;
    return TRUE;
}

// just send AT+CIFSR
int esp_myip(){
    return (ANS_OK == serial_sendwans("AT+CIFSR"));
}

// start server on given port
int esp_start_server(const char *port){
    if(ANS_OK != serial_sendwans("AT+CIPMUX=1")){ // can't start server without mux
        // what if already ready?
        char *x = serial_getline(NULL, NULL);
        if(!x || !serial_tidx(x, "link is builded")) return FALSE;
    }
    ESPMSG("AT+CIPSERVER=1,");
    if(ANS_OK != serial_sendwans(port)) return FALSE;
    return TRUE;
}

// stop server
int ep_stop_server(){
    return (ANS_OK == serial_sendwans("AT+CIPSERVER=0"));
}

// next (or only) line of received data
const char *esp_msgline(){
    DBG("receivedlen=%d", receivedlen);
    if(msgptr){
        const char *p = msgptr;
        msgptr = NULL;
        return p;
    }
    if(receivedlen < 1) return NULL;
    int l, d;
    const char *got = serial_getline(&l, &d);
    receivedlen -= l + d;
    return got;
}

// process connection/disconnection/messages
// fd - file descriptor of opened/closed connections
esp_clientstat_t esp_process(int *fd){
    msgptr = NULL;
    receivedlen = 0;
    int l, d;
    char *got = serial_getline(&l, &d);
    if(!got) return ESP_CLT_IDLE;
    const char *x = serial_tidx(got, "+IPD,");
    if(x){
        if(fd) *fd = serial_s2i(x);
        x = serial_tidx(x, ",");
        if(!x) return ESP_CLT_ERROR;
        int r = serial_s2i(x);
        x = serial_tidx(x, ":");
        if(!x) return ESP_CLT_ERROR;
        receivedlen = r - d - (l - (x-got)); // this is a rest of data (if any)
        msgptr = x;
        return ESP_CLT_GETMESSAGE;
    }
    // check for CONNECT/CLOSE
    if((x = serial_tidx(got, ",CONNECT"))){
        if(fd) *fd = serial_s2i(got);
        return ESP_CLT_CONNECTED;
    }
    if((x = serial_tidx(got, ",CLOSED"))){
        if(fd) *fd = serial_s2i(got);
        return ESP_CLT_DISCONNECTED;
    }
    DBG("Unknown message: '%s'", got);
    return ESP_CLT_IDLE;
}

int esp_send(int fd, const char *msg){
    DBG("send '%s' to %d", msg, fd);
    ESPMSG("AT+CIPSENDEX=");
    if(!serial_putchar('0' + fd)) return FALSE;
    if(ANS_OK != serial_sendwans(",2048")) return FALSE;
    int got = 0;
    // try several times
    for(int i = 0; i < 10; ++i){
        got = serial_getch();
        if(got == '>') break;
    }
    if(got != '>'){
        DBG("Didn't found '>'");
        serial_send_msg("\\0"); // terminate message
        serial_clr();
        return FALSE; // go into terminal mode
    }
    serial_clr(); // remove space after '>'
    ESPMSG(msg);
    if(ANS_OK == serial_sendwans("\\0")) return TRUE;
    DBG("Didn't sent");
    return FALSE;
}

void esp_reset(){
    serial_sendwans("AT+RST");
}

void esp_close(){
    serial_sendwans("AT+CIPMUX=0");
    serial_sendwans("AT+CIPCLOSE");
}


int esp_listAP(){
    if(ANS_OK == serial_sendwans("AT+CWLAP")) return TRUE;
    return FALSE;
}
