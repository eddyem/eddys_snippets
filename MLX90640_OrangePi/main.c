/*
 * This file is part of the mxl90640wPi project.
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

#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <usefull_macros.h>

#include "cmdlnopts.h"
#include "mlx90640.h"

#define DEVICE_ID 0x33

static glob_pars *GP = NULL;

void signals(int sig){
    if(sig){
        signal(sig, SIG_IGN);
        DBG("Get signal %d, quit.\n", sig);
    }
    LOGERR("Exit with status %d", sig);
    if(GP && GP->pidfile) // remove unnesessary PID file
        unlink(GP->pidfile);
    exit(sig);
}



int main (int argc, char **argv){
    initial_setup();
    char *self = strdup(argv[0]);
    GP = parse_args(argc, argv);
    check4running(self, GP->pidfile);
    FREE(self);
    if(GP->logfile) OPENLOG(GP->logfile, LOGLEVEL_ANY, 1);

    if(!mlx90640_init(GP->device, DEVICE_ID)) ERR("Can't open device");
    double *ima = NULL;
    if(!mlx90640_take_image(0, &ima) || !ima) ERRX("Can't take image");
  //  mlx90640_dump_parameters();
    double *ptr = ima;
    green("Image:\n");
    for(int row = 0; row < MLX_H; ++row){
        for(int col = 0; col < MLX_W; ++col){
            printf("%5.1f ", *ptr++);
        }
        printf("\n");
    }
    return 0;
}
