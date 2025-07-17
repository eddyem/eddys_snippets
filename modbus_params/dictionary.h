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

#include <stddef.h>
#include <stdint.h>

typedef struct{
    char *code;             // mnemonic code
    char *help;             // help message (all after '#')
    uint16_t value;         // value
    uint16_t reg;           // register number
    uint8_t readonly;       // ==1 if can't be changed
} dicentry_t;

typedef struct{
    char *name;             // alias name
    char *expr;             // expression to run
    char *help;             // help message
} alias_t;

int opendict(const char *dic);
void closedict();
int chkdict();
size_t get_dictsize();
char *dicentry_descr(dicentry_t *entry, char *buf, size_t bufsize);
char *dicentry_descrN(size_t N, char *buf, size_t bufsize);

dicentry_t *findentry_by_code(const char *code);
dicentry_t *findentry_by_reg(uint16_t reg);

int setdumppars(char **pars);
int opendumpfile(const char *name);
void closedumpfile();
int setDumpT(double dT);
int rundump();
char *getdumpname();

int read_dict_entries(const char *outdic);

int openaliases(const char *filename);
int chkaliases();
void closealiases();
size_t get_aliasessize();
alias_t *find_alias(const char *name);
char *alias_descr(alias_t *entry, char *buf, size_t bufsize);
char *alias_descrN(size_t N, char *buf, size_t bufsize);
