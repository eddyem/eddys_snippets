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

typedef enum{
    ANS_FAILED,
    ANS_OK,
    ANS_ERR
} serial_ans_t;

void serial_clr();
int serial_set_timeout(double tms);
int serial_init(char *path, int speed);
void serial_close();
int serial_send_msg(const char *msg);
int serial_send_cmd(const char *msg);
int serial_putchar(char ch);
serial_ans_t serial_sendwans(const char *msg);
char *serial_getline(int *len, int *deleted);
int serial_getch();

// conversion functions for ESP
const char *serial_tidx(const char *s, const char *t);
int serial_s2i(const char *s);
