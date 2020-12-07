#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <usefull_macros.h>

// 3 include-files for readline
#include <unistd.h>
#include <readline/readline.h>
#include <readline/history.h>

#define MAX_MSG_LEN (1023)

typedef struct msglist_{
    char *data;
    struct msglist_ *next, *last;
} msglist;

typedef enum{
    idxMOSI = 0,
    idxMISO = 1
} msgidx;

// index 0 - MOSI, index 1 - MISO
typedef struct{
    msglist *text[2];           // stringified text messages
    pthread_mutex_t mutex[2];   // text changing mutex
} message;

#define THREADNAMEMAXLEN    (31)
typedef struct{
    char name[THREADNAMEMAXLEN+1];
    message mesg;
    pthread_t thread;
    void *(*handler)(void *);
} threadinfo;

typedef struct thread_list_{
    threadinfo ti;
    struct thread_list_ *next;
} threadlist;

static threadlist *thelist = NULL;

/**
 * push data into the tail of a list (FIFO)
 * @param lst (io) - list
 * @param v (i)    - data to push
 * @return pointer to just pused node
 */
static msglist *pushmessage(msglist **lst, char *v){
    if(!lst || !v) return NULL;
    msglist *node;
    if((node = MALLOC(msglist, 1)) == 0)
        return NULL; // allocation error
    node->data = strdup(v);
    if(!node->data){
        FREE(node);
        return NULL;
    }
    if(!*lst){
        *lst = node;
        (*lst)->last = node;
        return node;
    }
    (*lst)->last->next = node;
    (*lst)->last = node;
    return node;
}

/**
 * @brief popmessage - get data from head of list
 * @param lst (io) - list
 * @return data from first node or NULL if absent  (SHOULD BE FREEd AFER USAGE!)
 */
static char *popmessage(msglist **lst){
    if(!lst || !*lst) return NULL;
    char *ret;
    msglist *node = *lst;
    if(node->next) node->next->last = node->last; // pop not last message
    ret = node->data;
    *lst = node->next;
    FREE(node);
    return ret;
}

static threadlist *getlast(){
    if(!thelist) return NULL;
    threadlist *t = thelist;
    while(t->next) t = t->next;
    return t;
}

// find thread by name
static threadinfo *findthread(char *name){
    if(!name) return NULL;
    if(!thelist) return NULL; // thread list is empty
    size_t l = strlen(name);
    if(l < 1 || l > THREADNAMEMAXLEN) return NULL;
    DBG("Try to find thread '%s'", name);
    threadlist *lptr = thelist;
    while(lptr){
        DBG("Check '%s'", lptr->ti.name);
        if(strcmp(lptr->ti.name, name) == 0) return &lptr->ti;
        lptr = lptr->next;
    }
    return NULL;
}

// add message to queue
static char *mesgAddText(msgidx idx, message *msg, char *txt){
    if(idx < idxMOSI || idx > idxMISO){
        WARNX("Wrong message index");
        return NULL;
    }
    int addn = 0;
    if(!msg) return NULL;
    size_t L = strlen(txt);
    if(L < 1) return NULL;
    DBG("Want to add mesg '%s' with length %zd", txt, L);
    if(pthread_mutex_lock(&msg->mutex[idx])) return NULL;
    if(!pushmessage(&msg->text[idx], txt)) return NULL;
    pthread_mutex_unlock(&msg->mutex[idx]);
    return msg->text[idx]->data;
}

// get all messages from queue (allocates data, should be free'd after usage!)
static char *mesgGetText(msgidx idx, message *msg){
    if(idx < idxMOSI || idx > idxMISO){
        WARNX("Wrong message index");
        return NULL;
    }
    if(!msg) return NULL;
    char *text = NULL;
    if(pthread_mutex_lock(&msg->mutex[idx])) return NULL;
    text = popmessage(&msg->text[idx]);
ret:
    pthread_mutex_unlock(&msg->mutex[idx]);
    return text;
}

/**
 * @brief registerThread - register new thread
 * @param name - thread name
 * @param handler - thread handler
 * @return pointer to new threadinfo struct or NULL if failed
 */
threadinfo *registerThread(char *name, void *(*handler)(void *)){
    threadinfo *ti = findthread(name);
    if(!name || strlen(name) < 1 || !handler) return NULL;
    DBG("Register new thread with name '%s'", name);
    if(ti){
        WARNX("Thread named '%s' exists!", name);
        return NULL;
    }
    if(!thelist){ // the first element
        thelist = MALLOC(threadlist, 1);
        ti = &thelist->ti;
    }else{
        threadlist *last = getlast();
        last->next = MALLOC(threadlist, 1);
        ti = &last->next->ti;
    }
    ti->handler = handler;
    snprintf(ti->name, 31, "%s", name);
    memset(&ti->mesg, 0, sizeof(ti->mesg));
    for(int i = 0; i < 2; ++i)
        pthread_mutex_init(&ti->mesg.mutex[i], NULL);
    if(pthread_create(&ti->thread, NULL, handler, (void*)ti)){
        WARN("pthread_create()");
        return NULL;
    }
    return ti;
}

// kill and unregister thread with given name; @return - if all OK
int killThread(const char *name){
    if(!name || !thelist) return 1;
    threadlist *lptr = thelist, *prev = NULL;
    for(; lptr; lptr = lptr->next){
        if(strcmp(lptr->ti.name, name)){
            prev = lptr;
            continue;
        }
        DBG("Found '%s', prev: '%s', delete", name, prev->ti.name);
        threadlist *next = lptr->next;
        if(lptr == thelist) thelist = next;
        else if(prev) prev->next = next;
        for(int i = 0; i < 2; ++i){
            pthread_mutex_lock(&lptr->ti.mesg.mutex[i]);
            char *txt;
            while((txt = popmessage(&lptr->ti.mesg.text[i]))) FREE(txt);
            pthread_mutex_destroy(&lptr->ti.mesg.mutex[i]);
        }
        if(pthread_cancel(lptr->ti.thread)) WARN("Can't kill thread '%s'", name);
        FREE(lptr);
        return 0;
    }
    return 2; // not found
}

static void *handler(void *data){
    threadinfo *ti = (threadinfo*)data;
    while(1){
        char *got = mesgGetText(idxMOSI, &ti->mesg);
        if(got){
            green("%s got: %s\n", ti->name, got);
            FREE(got);
            mesgAddText(idxMISO, &ti->mesg, "received");
            mesgAddText(idxMISO, &ti->mesg, "need more");
        }
        usleep(100);
    }
    return NULL;
}

static void dividemessages(message *msg, char *longtext){
    char *copy = strdup(longtext), *saveptr = NULL;
    for(char *s = copy; ; s = NULL){
        char *nxt = strtok_r(s, " ", &saveptr);
        if(!nxt) break;
        mesgAddText(idxMOSI, msg, nxt);
    }
    FREE(copy);
}

static void procmesg(char *text){
    if(!text) return;
    char *nxt = strchr(text, ' ');
    if(!nxt){
        WARNX("Usage: cmd data, where cmd:\n"
              "\tnew threadname - create thread\n"
              "\tdel threadname - delete thread\n"
              "\tsend threadname data - send data to thread\n"
              "\tsend all data - send data to all\n");
        return;
    }
    *nxt++ = 0;
    if(strcasecmp(text, "new") == 0){
        registerThread(nxt, handler);
    }else if(strcasecmp(text, "del") == 0){
        if(killThread(nxt)) WARNX("Can't delete '%s'", nxt);
    }else if(strcasecmp(text, "send") == 0){
        text = strchr(nxt, ' ');
        if(!text){
            WARNX("send all/threadname data");
            return;
        }
        *text++ = 0;
        if(strcasecmp(nxt, "all") == 0){ // bcast
            threadlist *lptr = thelist;
            while(lptr){
                threadinfo *ti = &lptr->ti;
                lptr = lptr->next;
                green("Bcast send '%s' to thread '%s'\n", text, ti->name);
                dividemessages(&ti->mesg, text);
            }
        }else{ // single
            threadinfo *ti = findthread(nxt);
            if(!ti){
                WARNX("Thread '%s' not found", nxt);
                return;
            }
            green("Send '%s' to thread '%s'\n", text, nxt);
            dividemessages(&ti->mesg, text);
        }
    }
}

int main(){
    using_history();
    while(1){
        threadlist *lptr = thelist;
        while(lptr){
            threadinfo *ti = &lptr->ti;
            lptr = lptr->next;
            char *got;
            while((got = mesgGetText(idxMISO, &ti->mesg))){
                red("got from '%s': %s\n", ti->name, got);
                fflush(stdout);
                FREE(got);
            }
        }
        char *text = readline("mesg > ");
        if(!text) break; // ^D
        if(strlen(text) < 1) continue;
        add_history(text);
        procmesg(text);
        FREE(text);
    }
    return 0;
}

