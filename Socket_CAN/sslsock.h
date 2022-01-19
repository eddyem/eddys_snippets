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

#pragma once
#ifndef SSLSOCK_H__
#define SSLSOCK_H__

#if ! defined CLIENT && ! defined SERVER
#error "Define CLIENT or SERVER before including this file"
#endif
#if defined CLIENT && defined SERVER
#error "Both CLIENT and SERVER defined"
#endif

#define BACKLOG     10

int open_socket();


#endif // SSLSOCK_H__
