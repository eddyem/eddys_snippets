/*
 * This file is part of the NES_web project.
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

#include <crypt.h>
#include <errno.h>
#include <onion/block.h>
#include <onion/exportlocal.h>
#include <onion/handler.h>
#include <onion/log.h>
#include <onion/onion.h>
#include <onion/shortcuts.h>
#include <onion/types_internal.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <usefull_macros.h>

#include "auth.h"
#include "cmdlnopts.h"
#include "websockets.h"

// temporary
#define putlog(...)

onion_connection_status get(_U_ onion_handler *h, onion_request *req, onion_response *res){
    sessinfo *session = qookieSession(req);
    if(!session){
        onion_response_write0(res, "NeedAuth");
        ONION_WARNING("try to run command without auth");
        return OCS_FORBIDDEN;
    }
    const char *path = onion_request_get_path(req);
    if(path && *path == 0) path = NULL;
    else printf("GET ask path %s\n", path);
    const char *param = getQdata(req, "param");
    if(param) printf("User set param=%s\n", param);
    if(getQdata(req, "getWSkey")){
        addWSkey(res, session);
    }
    freeSessInfo(&session);
    return OCS_CLOSE_CONNECTION;
}

static onion *os = NULL, *ow = NULL;
void signals(int signo){
    closeSQLite();
    if(os) onion_free(os);
    if(ow) onion_free(ow);
    exit(signo);
}

// POST/GET server
static void *runPostGet(_U_ void *data){
    os = onion_new(O_THREADED);
    if(!(onion_flags(os) & O_SSL_AVAILABLE)){
        ONION_ERROR("SSL support is not available");
        signals(1);
    }
    int error = onion_set_certificate(os, O_SSL_CERTIFICATE_KEY, G.certfile, G.keyfile);
    if(error){
        ONION_ERROR("Cant set certificate and key files (%s, %s)", G.certfile, G.keyfile);
        signals(1);
    }
    onion_set_port(os, G.port);
    onion_url *url = onion_root_url(os);
    onion_url_add_handler(url, "^static/", onion_handler_export_local_new("static"));
    onion_url_add_with_data(url, "", onion_shortcut_internal_redirect, "static/index.html", NULL);
    onion_url_add(url, "^auth/", auth);
    onion_url_add(url, "^get/", get);
    error = onion_listen(os);
    if(error) ONION_ERROR("Cant create POST/GET server: %s", strerror(errno));
    onion_free(os);
    return NULL;
}

// Websocket server
static void *runWS(_U_ void *data){
    ow = onion_new(O_THREADED);
    if(!(onion_flags(ow) & O_SSL_AVAILABLE)){
        ONION_ERROR("SSL support is not available");
        signals(1);
    }
    int error = onion_set_certificate(ow, O_SSL_CERTIFICATE_KEY, G.certfile, G.keyfile);
    if(error){
        ONION_ERROR("Cant set certificate and key files (%s, %s)", G.certfile, G.keyfile);
        signals(1);
    }
    onion_set_port(ow, G.wsport);
    onion_url *url = onion_root_url(ow);
    onion_url_add(url, "", websocket_run);
    DBG("Listen websocket");
    error = onion_listen(ow);
    if(error) ONION_ERROR("Cant create POST/GET server: %s", strerror(errno));
    onion_free(ow);
    return NULL;
}

static void runServer(){
    // if(G.logfilename) Cl_createlog();
    signal(SIGTERM, signals);
    signal(SIGINT, signals);
    signal(SIGQUIT, signals);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGHUP, SIG_IGN);

    pthread_t pg_thread, ws_thread;
    if(pthread_create(&pg_thread, NULL, runPostGet, NULL)){
        ERR("pthread_create()");
    }
    if(pthread_create(&ws_thread, NULL, runWS, NULL)){
        ERR("pthread_create()");
    }
    do{
        if(pthread_kill(pg_thread, 0) == ESRCH){ // POST/GET died
            WARNX("POST/GET server thread died");
            putlog("POST/GET server thread died");
            pthread_join(pg_thread, NULL);
            if(pthread_create(&pg_thread, NULL, runPostGet, NULL)){
                putlog("pthread_create() failed");
                ERR("pthread_create()");
            }
        }
        if((pthread_kill(pg_thread, 0) == ESRCH) || (pthread_kill(ws_thread, 0) == ESRCH)){ // died
            WARNX("Websocket server thread died");
            putlog("Websocket server thread died");
            pthread_join(ws_thread, NULL);
            if(pthread_create(&ws_thread, NULL, runWS, NULL)){
                putlog("pthread_create() failed");
                ERR("pthread_create()");
            }
        }
#if 0
        usleep(1000); // sleep a little or thread's won't be able to lock mutex
        if(dtime() - tgot < T_INTERVAL) continue;
        tgot = dtime();
        /*
         * INSERT CODE HERE
         * Gather data (poll_device)
         */
        // copy temporary buffers to main
        pthread_mutex_lock(&mutex);
        /*
         * INSERT CODE HERE
         * fill global data buffers
         */
        pthread_mutex_unlock(&mutex);
#endif
    }while(1);
    putlog("Unreaceable code reached!");
    ERRX("Unreaceable code reached!");
}

static char *getl(char *msg, int noempty){
    static char *inpstr = NULL;
    do{
        FREE(inpstr);
        printf("Enter %s: ", msg);
        size_t n = 0;
        if(getline(&inpstr, &n, stdin) < 0) FREE(inpstr);
        else{
            char *nl = strchr(inpstr, '\n');
            if(nl) *nl = 0;
            if(strlen(inpstr) < 1 && noempty){
                WARNX("Enter non-empty string");
            }else break;
        }
    }while(1);
    return inpstr;
}

static void adduser(){
    userinfo *User = MALLOC(userinfo, 1);
    char *str;
    do{
        str = getl("username", 1);
        printf("Username: %s\n", str);
        User->username = strdup(str);
        do{
            str = getl("password", 1);
            if(strlen(str) < 4){
                WARNX("Bad password: not less than 4 symbols");
            }else break;
        }while(1);
        User->password = strdup(crypt(str, "$6$"));
        printf("PassHash: %s\n", User->password);
        str = getl("access level", 1);
        User->level = atoi(str);
        printf("Level: %d\n", User->level);
        str = getl("comment", 0);
        if(strlen(str)){
            User->comment = strdup(str);
            printf("Comment: %s", User->comment);
        }
        if(addUser(User)) continue;
        if(!(str = getl("next? (y/n)", 0)) || 0 != strcmp(str, "y")) break;
    }while(1);
    freeUserInfo(&User);
}

extern char *onion_sessions_generate_id();

int main(int argc, char **argv){
    initial_setup();
    parse_args(argc, argv);
    if(!G.usersdb) ERRX("You should point users database file name");
    if(!G.sessiondb) ERRX("You should point session database file name");
    if(initSQLite(G.usersdb, G.sessiondb)) return 1;
    if(G.vacuum){
        printf("VAAAA\n");
        vacuumSQLite();
    }
    if(G.dumpUserDB) showAllUsers();
    if(G.userdel){
        for(int i = 0; G.userdel[i]; ++i){
            if(!deleteUser(G.userdel[i])) green("User %s deleted\n", G.userdel[i]);
        }
    }
    if(G.useradd) adduser();
    if(G.dumpSessDB) showAllSessions();
    if(G.delsession){
        if(!deleteSession(G.delsession))
            green("Session with ID %s deleted\n", G.delsession);
    }
    if(G.delatime){
        if(G.delatime < 0) G.delatime = time(NULL) - 31556926LL;
        if(!deleteOldSessions((int64_t)G.delatime))
            green("All sessions with atime<%lld deleted\n", G.delatime);
    }
    if(G.runServer){
        if(strcmp(G.port, G.wsport) == 0) ERRX("Server port ans websocket port should be different!");
        runServer();
    }
    closeSQLite();
    return 0;
}
