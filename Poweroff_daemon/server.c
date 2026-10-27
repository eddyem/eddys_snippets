/*
 * This file is part of the poweroff_daemon project.
 * Copyright 2026 Edward V. Emelianov <edward.emelianoff@gmail.com>.
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

#include <usefull_macros.h>

#include "serial.h"
#include "server.h"

extern bool isrunning;
static sl_sock_t *s = NULL;

static sl_sock_hresult_e defhandler(struct sl_sock *s, const char *str){
    if(!s || !str) return RESULT_FAIL;
    char buf[BUFSIZ];
    if(!send_custom_command(str, buf, BUFSIZ)) return false;
    LOGDBG("Send command '%s', answer:\n%s\n", str, buf);
    sl_sock_sendstrmessage(s, buf);
    return RESULT_SILENCE;
}

static sl_sock_hresult_e helphandler(struct sl_sock *s, sl_sock_hitem_t _U_ *item, const char _U_ *req){
    if(!s) return RESULT_FAIL;
    char buf[BUFSIZ];
    if(!send_custom_command("help", buf, BUFSIZ)) return false;
    LOGDBG("Send command 'help', answer:\n%s\n", buf);
    sl_sock_sendstrmessage(s, buf);
    return RESULT_SILENCE;
}

static sl_sock_hitem_t handlers[] = {
    {sl_comment, NULL, "    Enter any message for USB", NULL},
    {helphandler, "_help", "Send 'help' to serial", NULL},
    {NULL, NULL, NULL, NULL}
};

bool run_server(const char *sockpath){
    if(!sockpath) return false;
    s = sl_sock_run_server(SOCKT_UNIX, sockpath, -1, handlers);
    if(!s) return false;
    sl_sock_defmsghandler(s, defhandler);
    return true;
}

void kill_server(){
    if(!s) return;
    sl_sock_delete(&s);
}
