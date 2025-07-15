/*
 * This file is part of the modbus_param project.
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

#include <stdint.h>

typedef struct{
    char *code;             // mnemonic code
    uint16_t value;         // value
    uint16_t reg;           // register number
    uint8_t readonly;       // ==1 if can't be changed
} dicentry_t;

int setdumppars(char **pars);
int opendumpfile(const char *name);
void closedumpfile();
int setDumpT(double dT);
int rundump();
char *getdumpname();

int opendict(const char *dic);

dicentry_t *findentry_by_code(const char *code);
dicentry_t *findentry_by_reg(uint16_t reg);

int open_modbus(const char *path, int baudrate);
void close_modbus();
int set_slave(int ID);

int read_entry(dicentry_t *entry);
int write_entry(dicentry_t *entry);
int write_regval(char **keyval);
int write_codeval(char **codeval);

int dumpall(const char *outdic);
void dumpregs(int **addresses);
void dumpcodes(char **keycodes);
