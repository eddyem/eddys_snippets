/*
 * This file is part of the poweroff_daemon project.
 * Copyright 2026 Edward V. Emelianov <edward.emelianoff@gmail.com>.
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

#define CMD_BTNCHECK        "B\n"
#define ANS_BTN0STATE       "BUTTON0"

#define CMD_GETVOLTAGE      "V\n"
#define ANS_V12             "V12"
#define LOG_V12             1150
#define LOG_VOLTAGE_DT      60
#define LOWEST_V12          970

#define MAX_WERR_CNT        5
#define MAX_RERR_CNT        15

bool open_serial(const char *path, int baudrate);
void close_serial();

bool check_buttons();
bool check_voltage();
bool send_custom_command(const char *command, char *answer, size_t anslen);
