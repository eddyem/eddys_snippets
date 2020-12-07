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

#include <linux/limits.h> // PATH_MAX
#include <stdio.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "aux.h"
#include "cmdlnopts.h"

// print messages for given verbose_level
int verbose(verblevel levl, const char *fmt, ...){
    if((unsigned)verbose_level < levl) return 0;
    va_list ar; int i;
    va_start(ar, fmt);
    i = vprintf(fmt, ar);
    va_end(ar);
    printf("\n");
    fflush(stdout);
    return i;
}

/**
 * @brief check_filename - find file name "outfile_xxxx.suff" NOT THREAD-SAFE!
 * @param outfile - file name prefix
 * @param suff    - file name suffix
 * @return NULL or next free file name like "outfile_0010.suff" (don't free() it!)
 */
char *check_filename(char *outfile, char *suff){
    static char buff[PATH_MAX];
    struct stat filestat;
    int num;
    for(num = 1; num < 10000; num++){
        if(snprintf(buff, PATH_MAX, "%s_%04d.%s", outfile, num, suff) < 1)
            return NULL;
        if(stat(buff, &filestat)) // OK, file not exists
            return buff;
    }
    return NULL;
}
