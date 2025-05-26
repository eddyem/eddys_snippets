/*
 * This file is part of the modbus_relay project.
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

#include <modbus/modbus.h>

// timeout for socket closing if nothing received
#define SOCKET_TIMEOUT  (60.0)
// timeout to turn relays off
#define RELAYS_TIMEOUT  (10.0)
// time interval for MODBUS data polling (seconds)
#define T_INTERVAL      (1.)

void runserver(const char *port, modbus_t *ctx);
int closeserver();
