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

#pragma once

// really only 5 clients allowed
#define ESP_MAX_CLT_NUMBER  8

typedef enum{
    ESP_CIP_0,
    ESP_CIP_1,
    ESP_CIP_GOTIP,
    ESP_CIP_CONNECTED,
    ESP_CIP_DISCONNECTED,
    ESP_CIP_FAILED
} esp_cipstatus_t;

typedef enum{
    ESP_CLT_IDLE,           // nothing happened
    ESP_CLT_CONNECTED,      // new client connected
    ESP_CLT_DISCONNECTED,   // disconnected
    ESP_CLT_ERROR,          // error writing or other
    ESP_CLT_GETMESSAGE,     // receive message from client
    ESP_CLT_OK,             // sent OK
} esp_clientstat_t;

int esp_check();
void esp_close();
void esp_reset();
esp_cipstatus_t esp_cipstatus();
int esp_connect(const char *SSID, const char *pass);
int esp_myip();
int esp_start_server(const char *port);
int ep_stop_server();
esp_clientstat_t esp_process(int *fd);
int esp_send(int fd, const char *msg);
const char *esp_msgline();
int esp_listAP();
