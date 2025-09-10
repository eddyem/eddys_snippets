/*
 * This file is part of the mlxtest project.
 * Copyright 2022 Edward V. Emelianov <edward.emelianoff@gmail.com>.
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

#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <usefull_macros.h>

#include "mlx90640.h"
#include "testdata.h"

int main (int _U_ argc, char _U_ **argv){
    sl_init();
    MLX90640_params p;
    if(!get_parameters(EEPROM, &p)) ERRX("Can't get parameters from test data");
    dump_parameters(&p, &extracted_parameters);
    fp_t *sp;
    for(int i = 0; i < 2; ++i){
        printf("Process subpage %d\n", i);
        sp = process_subpage(&p, DataFrame[i], i, 2);
        if(!sp) ERRX("WTF?");
        dumpIma(sp);
        chkImage(sp, ToFrame[i]);
    }
    drawIma(sp);
    return 0;
}
