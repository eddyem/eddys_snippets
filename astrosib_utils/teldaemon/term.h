/*                                                                                                  geany_encoding=koi8-r
 * term.h
 *
 * Copyright 2018 Edward V. Emelianov <eddy@sao.ru, edward.emelianoff@gmail.com>
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
#ifndef __TERM_H__
#define __TERM_H__

#define FRAME_MAX_LENGTH        (300)
#define MAX_MEMORY_DUMP_SIZE    (0x800 * 4)
// Terminal timeout (seconds)
#define     WAIT_TMOUT          (0.5)
// Terminal polling timeout - 1 second
#define     T_POLLING_TMOUT     (1.0)

void run_terminal();
void try_connect(char *device);
char *poll_device();
int ping();
int write_cmd(const char *cmd);

#endif // __TERM_H__
