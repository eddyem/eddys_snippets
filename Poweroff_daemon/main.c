/*
 * monitor_daemon.c - daemon that monitors serial device /dev/ttyUSB0.
 * It sends commands "B\n" and "V\n", parses responses, and triggers poweroff
 * if BUTTON0 is pressed or V12 is below 9700.
 *
 * Protocol uses only '\n' as line separator (no '\r').
 * USB CDC device: port settings (baudrate, format) are irrelevant.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <linux/prctl.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <time.h>
#include <usefull_macros.h>

#include "cmdlnopts.h"
#include "serial.h"
#include "server.h"

static cmdln_t *G = NULL;
bool isrunning = false;
static pid_t childpid = 0;

void signals(int sig){
    if(sig > 0){
        signal(sig, SIG_IGN);
        DBG("Get signal %d, quit.\n", sig);
    }
    if(childpid){
        LOGERR("Main process exits with status %d", sig);
        if(G->pidfile) // remove unnesessary PID file
            unlink(G->pidfile);
    }else{
        isrunning = false;
        sleep(2); // let child to graceful out
    }
    exit(sig);
}

int main(int argc, char **argv){
    sl_init();
    G = parseargs(&argc, &argv);
    sl_check4running(NULL, G->pidfile);

    sl_loglevel_e lvl = LOGLEVEL_WARN;
    if(G->verbose){
        lvl += G->verbose;
        if(lvl >= LOGLEVEL_AMOUNT) lvl = LOGLEVEL_ANY;
    }

    /* Open log file before daemonizing */
    if(G->logfile){
        OPENLOG(G->logfile, lvl, 1);
        if(!sl_globlog) WARNX("Can't create log file %s", G->logfile);
    }

    /* Daemonize */
    if(sl_daemonize() != 0){
        ERR("daemonize failed");
    }

    /* Set global timeout for sl_tty_read() */
    sl_tty_tmout(G->timeout_us);

    LOGMSG("Monitor daemon started, polling every %.2f seconds", G->poll_interval);
    signal(SIGTERM, signals);
    signal(SIGINT, signals);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGQUIT, signals);

    time_t lastd = 0;
    /* Main loop */
    while(true){
        childpid = fork();
        if(childpid < 0){
            LOGERR("fork() returns error");
            sleep(5);
            continue;
        }
        if(childpid){
            DBG("Created child with PID %d\n", childpid);
            wait(NULL);
            time_t t = time(NULL);
            if(t - lastd > 600){ // at least 10 minutes of work
                LOGERR("Child %d died\n", childpid);
            }
            lastd = t;
            WARNX("Child %d died\n", childpid);
            sleep(5);
        }else{
            prctl(PR_SET_PDEATHSIG, SIGTERM); // send SIGTERM to child when parent dies
            break; // go out to normal functional
        }
    }

    /******************** child PID ********************/
    // Open serial port
    if(!open_serial(G->serialdev, G->baudrate)){
        LOGERR("Can't open %s @%d", G->serialdev, G->baudrate);
        ERRX("Can't open %s @%d", G->serialdev, G->baudrate);
    }

    isrunning = true;

    if(!run_server(G->sockpath)){
        LOGERR("Can't run server @ %s", G->sockpath);
        ERRX("Can't run server @ %s", G->sockpath);
    }

    while(isrunning){
        bool trigger = false;
        if(check_buttons()) trigger = true;
        if(check_voltage()) trigger = true;

        if(trigger){
            LOGWARN("Trigger condition met, executing poweroff");
            DBG("poweroff");
            int ret = system("/sbin/poweroff");
            if(ret == -1){
                LOGERR("Failed to execute poweroff: %s", strerror(errno));
            }else{
                LOGWARN("Poweroff called");
                break;
            }
        }

        usleep((useconds_t)(G->poll_interval * 1e6)); // seconds -> useconds
    }

    kill_server();

    close_serial();

    return 0;
}
