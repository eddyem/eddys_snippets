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

static double image[MLX_PIXNO];
static double image2[MLX_PIXNO];
static void pushima(const double *img){
    for(int i = 0; i < MLX_PIXNO; ++i){
        double val = *img++;
        image[i] += val;
        image2[i] += val*val;
    }
}

int main (int argc, char **argv){
    initial_setup();
    char *self = strdup(argv[0]);
    GP = parse_args(argc, argv);
    check4running(self, GP->pidfile);
    FREE(self);
    if(GP->logfile) OPENLOG(GP->logfile, LOGLEVEL_ANY, 1);

    if(!mlx90640_init(GP->device, DEVICE_ID)) ERR("Can't open device");
    //mlx90640_dump_parameters();
#define N 9
    double T0 = dtime();
    uint8_t simple = 2;
    //for(uint8_t simple = 0; simple < 3; ++simple){
        memset(image, 0, sizeof(image));
        memset(image2, 0, sizeof(image));
        for(int i = 0; i < N; ++i){
            double *ima = NULL;
            if(!mlx90640_take_image(simple, &ima) || !ima) ERRX("Can't take image");
            pushima(ima);
            printf("Got image %d, T=%g\n", i, dtime() - T0);
        }
        double *im = image, *im2 = image2;
        green("\nImage (simple=%d):\n", simple);
        for(int row = 0; row < MLX_H; ++row){
            for(int col = 0; col < MLX_W; ++col){
                double v = *im++, v2 = *im2;
                v /= N; v2 /= N;
                printf("%5.1f ", v);
                *im2++ = v2 - v*v;
            }
            printf("\n");
        }
        green("\nRMS:\n");
        im2 = image2;
        for(int row = 0; row < MLX_H; ++row){
            for(int col = 0; col < MLX_W; ++col){
                printf("%5.1f ", *im2++);
            }
            printf("\n");
        }
    //}
    return 0;
}
