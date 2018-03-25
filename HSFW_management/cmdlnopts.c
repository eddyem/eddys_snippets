/*
 * cmdlnopts.c - the only function that parse cmdln args and returns glob parameters
 *
 * Copyright 2013 Edward V. Emelianoff <eddy@sao.ru>
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
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <math.h>
#include "cmdlnopts.h"
#include "usefull_macros.h"

#define RAD 57.2957795130823
#define D2R(x) ((x) / RAD)
#define R2D(x) ((x) * RAD)

/*
 * here are global parameters initialisation
 */
int help;
glob_pars G;

int listNms   = LIST_NONE   // list names
    ,gohome   = 0           // first go home
    ,reName   = 0           // rename wheels/positions
    ,showpos  = 0           // show current position (if none args)
    ,setdef   = 0           // reset all names to default values
;


//            DEFAULTS
// default global parameters
glob_pars const Gdefault = {
     NULL          // wheelID
    ,NULL          // wheelName
    ,NULL          // serial
    ,0             // filterPos ( 0 - get current position)
    ,NULL          // filterName
    ,NULL          // filterId
};

/*
 * Define command line options by filling structure:
 *  name    has_arg flag    val     type        argptr          help
*/
myoption cmdlnopts[] = {
    /// "отобразить эту справку"
    {"help",    NO_ARGS,    NULL,   'h',    arg_int,    APTR(&help),        N_("show this help")},
    /// "буквенный идентификатор колеса"
    {"wheel-id",NEED_ARG,   NULL,   'W',    arg_string, APTR(&G.wheelID),   N_("letter wheel identificator")},
    /// "название колеса"
    {"wheel-name",NEED_ARG, NULL,   'N',    arg_string, APTR(&G.wheelName), N_("wheel name")},
    /// "серийный номер турели (с начальными нулями)"
    {"serial",  NEED_ARG,   NULL,   's',    arg_string, APTR(&G.serial),    N_("turret serial (with leading zeros)")},
    /// "номер позиции фильтра"
    {"f-position",NEED_ARG, NULL,   'p',    arg_int,    APTR(&G.filterPos), N_("filter position number")},
    /// "название фильтра"
    {"filter-name",NEED_ARG,NULL,   'n',    arg_string, APTR(&G.filterName),N_("filter name")},
    /// "идентификатор фильтра, например, \"A3\""
    {"filter-id",NEED_ARG,  NULL,   'i',    arg_string, APTR(&G.filterId),  N_("filter identificator like \"A3\"")},
    /// "список всех сохраненных имен"
    {"list-all",NO_ARGS,    &listNms,LIST_ALL,arg_none, NULL,               N_("list all stored names")},
    /// "список имен только присутствующих устройств"
    {"list",    NO_ARGS,    &listNms,LIST_PRES,arg_none,NULL,               N_("list only present devices' names")},
    /// "переместиться в стартовую позицию"
    {"home",    NO_ARGS,    NULL,   'H',    arg_none,   &gohome,            N_("move to home position")},
    /// "переименовать сохраненные имена колес/фильтров"
    {"rename",  NO_ARGS,    &reName,1,      arg_none,   NULL,               N_("rename stored wheels/filters names")},
    {"resetnames",NO_ARGS,  &setdef,1,      arg_none,   NULL,               N_("reset all names to default values")},
    end_option
};

/**
 * Safely convert data from string to double
 *
 * @param num (o) - double number read from string
 * @param str (i) - input string
 * @return TRUE if success
 */
bool myatod(double *num, const char *str){
    double res;
    char *endptr;
    assert(str);
    res = strtod(str, &endptr);
    if(endptr == str || *str == '\0' || *endptr != '\0'){
        /// "Неправильный формат числа double!"
        WARNX(_("Wrong double number format!"));
        return FALSE;
    }
    *num = res;
    return TRUE;
}

/**
 * Parse command line options and return dynamically allocated structure
 *      to global parameters
 * @param argc - copy of argc from main
 * @param argv - copy of argv from main
 * @return allocated structure with global parameters
 */
glob_pars *parse_args(int argc, char **argv){
    int i, oldargc = argc - 1;
    void *ptr;
    ptr = memcpy(&G, &Gdefault, sizeof(G)); assert(ptr);
    /// "Использование: %s [аргументы] [префикс выходного файла]\n\n\tГде [аргументы]:\n"
    //change_helpstring(_("Usage: %s [args] [outfile prefix] [file list for group operations]\n\n\tWhere args are:\n"));
    // parse arguments
    parseargs(&argc, &argv, cmdlnopts);
    if(help) showhelp(-1, cmdlnopts);
    if(argc > 0){
        /// "Игнорируются параметры: "
        WARNX(_("Ignore parameters:"));
        for (i = 0; i < argc; i++){
            printf("%s\n", argv[i]);
        }
    }
    if(argc == oldargc){ // no parameters given or given only wrong parameters
        //showpos = 1;
        listNms = LIST_SHORT; // short list of turrets present
    }
    return &G;
}

