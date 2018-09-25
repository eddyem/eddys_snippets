/*
 * main.h
 *
 * Copyright 2014 Edward V. Emelianov <eddy@sao.ru, edward.emelianoff@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 */
#pragma once
#ifndef __MAIN_H__
#define __MAIN_H__

#include <stdio.h>
#include <libintl.h>
#include <locale.h>
#include <assert.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <err.h>
#include <unistd.h>
#include <pthread.h>

#include "cmdlnopts.h"
#include "usefull_macros.h"

// global parameters
extern glob_pars *Global_parameters;

#endif // __MAIN_H__
