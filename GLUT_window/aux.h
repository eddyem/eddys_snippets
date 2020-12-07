/*
 * This file is part of the opengl project.
 * Copyright 2020 Edward V. Emelianov <edward.emelianoff@gmail.com>.
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
#ifndef AUX_H__
#define AUX_H__

typedef enum{
    VERB_NONE,
    VERB_MESG,
    VERB_DEBUG
} verblevel;

int verbose(verblevel levl, const char *fmt, ...);
char *check_filename(char *outfile, char *suff);

#define VMESG(...)  do{verbose(VERB_MESG, __VA_ARGS__);}while(0)
#define VDBG(...)   do{verbose(VERB_DEBUG, __VA_ARGS__);}while(0)

#endif // AUX_H__
