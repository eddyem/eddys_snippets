/*
 * que.h
 *
 * Copyright 2016 Edward V. Emelianov <eddy@sao.ru, edward.emelianoff@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 */
#pragma once
#ifndef __QUE_H__
#define __QUE_H__

#define MESSAGE_LEN          (512)
#define MESSAGE_QUE_SIZE   (16)


typedef enum{
	UNDEFINED = 0,
	SALT_SENT,
	VERIFIED,
}client_state;

typedef struct {
	int num;
	int idxwr;
	int idxrd;
	char message[MESSAGE_QUE_SIZE][MESSAGE_LEN];
	int already_connected;
	client_state state;
} control_data;

extern char que[];
extern control_data global_que;
void glob_que(char *buf);
void put_message_to_que(char *msg, control_data *dat);
char *get_message_from_que(control_data *dat);

//#define GLOB_MESG(...)  do{snprintf(que, 512, __VA_ARGS__); glob_que();}while(0)
#endif // __QUE_H__
