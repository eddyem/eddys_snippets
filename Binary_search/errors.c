/*
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

#include "errors.h"
#include <stdlib.h>
#include <stdio.h>

static const abortcodes AC[] = {
    //while read l; do N=$(echo $l|awk '{print $1 $2}'); R=$(echo $l|awk '{$1=$2=""; print substr($0,3)}'|sed 's/\.//'); echo -e "{0x$N, \"$R\"},"; done < codes.b
    {0x05030000, "Toggle bit not alternated"},
    {0x05040000, "SDO protocol timed out"},
    {0x05040001, "Client/server command specifier not valid or unknown"},
    {0x05040002, "Invalid block size (block mode only)"},
    {0x05040003, "Invalid sequence number (block mode only)"},
    {0x05040004, "CRC error (block mode only)"},
    {0x05040005, "Out of memory"},
    {0x06010000, "Unsupported access to an object"},
    {0x06010001, "Attempt to read a write only object"},
    {0x06010002, "Attempt to write a read only object"},
    {0x06020000, "Object does not exist in the object dictionary"},
    {0x06040041, "Object cannot be mapped to the PDO"},
    {0x06040042, "The number and length of the objects to be mapped would exceed PDO length"},
    {0x06040043, "General parameter incompatibility reason"},
    {0x06040047, "General internal incompatibility in the device"},
    {0x06060000, "Access failed due to a hardware error"},
    {0x06070010, "Data type does not match; length of service parameter does not match"},
    {0x06070012, "Data type does not match; length of service parameter too high"},
    {0x06070013, "Data type does not match; length of service parameter too low"},
    {0x06090011, "Sub-index does not exist"},
    {0x06090030, "Value range of parameter exceeded (only for write access)"},
    {0x06090031, "Value of parameter written too high"},
    {0x06090032, "Value of parameter written too low"},
    {0x06090036, "Maximum value is less than minimum value"},
    {0x08000000, "General error"},
    {0x08000020, "Data cannot be transferred or stored to the application"},
    {0x08000021, "Data cannot be transferred or stored to the application because of local control"},
    {0x08000022, "Data cannot be transferred or stored to the application because of the present device state"},
    {0x08000023, "Object dictionary dynamic generation fails or no object dictionary is present"},
};

const int ACmax = sizeof(AC)/sizeof(abortcodes) - 1;

const char *ACtext(uint32_t abortcode, int *n){
    int idx = ACmax/2, min_ = 0, max_ = ACmax, newidx = 0, iter=0;
    do{
        ++iter;
        uint32_t c = AC[idx].code;
        printf("idx=%d, min=%d, max=%d\n", idx, min_, max_);
        if(c == abortcode){
            if(n) *n = iter;
            return AC[idx].errmsg;
        }else if(c > abortcode){
            newidx = (idx + min_)/2;
            max_ = idx;

        }else{
            newidx = (idx + max_ + 1)/2;
            min_ = idx;
        }
        if(newidx == idx || min_ < 0 || max_ > ACmax){
            if(n) *n = 0;
            return NULL;
        }
        idx = newidx;
    }while(1);
}

void check_all(){
    int iter = 0, N;
    for(int i = 0; i <= ACmax; ++i){
        printf("code 0x%X: %s\n\n", AC[i].code, ACtext(AC[i].code, &N));
        iter += N;
    }
    printf("\n\ntotal: %d iterations, mean: %d, (%d for direct lookup)\n", iter, iter/(ACmax+1), (ACmax+1)/2);
}

int main(int argc, char **argv){
    if(argc != 2){
        check_all();
        return 0;
    }
    uint32_t x = (uint32_t)strtol(argv[1], NULL, 0);
    printf("x=0x%X\n", x);
    const char *text = ACtext(x, NULL);
    if(text) printf("%s\n", text);
    else printf("Unknown error code\n");
    return 0;
}
