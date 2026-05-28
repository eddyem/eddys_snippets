/*
 * que.c
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
#include <string.h>
#include "que.h"

char que[MESSAGE_LEN];
control_data global_que;

void put_message_to_que(char *msg, control_data *dat){
	int L = strlen(msg);
	if(dat->num >= MESSAGE_QUE_SIZE) return;
	++dat->num;
	if(L < 1 || L > MESSAGE_LEN - 1) L = MESSAGE_LEN - 1;
	strncpy(dat->message[dat->idxwr], msg, L);
	dat->message[dat->idxwr][L] = 0;
	if((++(dat->idxwr)) >= MESSAGE_QUE_SIZE) dat->idxwr = 0;
}

void glob_que(char *buf){
	put_message_to_que(buf, &global_que);
}

char *get_message_from_que(control_data *dat){
	char *R = dat->message[dat->idxrd];
	if(dat->num <= 0) return NULL;
	if((++dat->idxrd) >= MESSAGE_QUE_SIZE) dat->idxrd = 0;
	--dat->num;
	return R;
}
