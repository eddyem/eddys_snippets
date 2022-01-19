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
#include <inttypes.h>
#include <onion/dict.h>
#include <onion/log.h>
#include <onion/types_internal.h>
#include <pthread.h>
#include <sqlite3.h>
#include <stdio.h>
#include <string.h>
#include <usefull_macros.h>

#include "auth.h"

extern char *onion_sessions_generate_id();
extern void onion_random_init();

typedef struct{
    sqlite3 *db;            // session database
    char *name;             // database filename
    sqlite3_stmt *add;      // template to add/modify session
    sqlite3_stmt *getsid;   // get data by session ID
    sqlite3_stmt *getsockid;// get data by socked ID
    sqlite3_stmt *delold;   // delete all old sessions (with atime < XX)
    sqlite3_stmt *del;      // delete session
    pthread_mutex_t mutex;  // locking mutex
} _sessionDB;

typedef struct{
    sqlite3 *db;            // auth database
    char *name;             // database filename
    sqlite3_stmt *add;      // add user data
    sqlite3_stmt *get;      // get user data by username
    sqlite3_stmt *del;      // delete user data
    pthread_mutex_t mutex;  // locking mutex
} _authDB;

static _sessionDB *sessionDB = NULL;
static _authDB *authDB = NULL;

/**
 * @brief getQdata - find data in POST or GET parts
 * @param req - request
 * @param key - searching key
 * @return value or NULL
 */
const char *getQdata(onion_request *req, const char *key){
    if(!req || !key) return NULL;
    printf("key %s, ", key);
    const char *data = onion_request_get_query(req, key);
    printf("GET: '%s'  ", data);
    if(!data){
        data = onion_request_get_post(req, key);
        if(data && *data == 0) data = NULL;
        printf("POST: '%s'", data);
    }
    printf("\n");
    fflush(stdout);
    return data;
}

/**
 * @brief sqprep - prepare SQLite statement
 * @param db   - database
 * @param str  - text string for statement
 * @param stmt - the statement
 * @return 0 if all OK
 */
static int sqprep(sqlite3 *db, const char *str, sqlite3_stmt **stmt){
    int rc = sqlite3_prepare_v2(db, str, (int)strlen(str), stmt, NULL);
    if(rc != SQLITE_OK){
        WARNX("Can't prepare statement to save (%d)", rc);
        return 1;
    }
    return 0;
}

/**
 * @brief addtext - bind text phrase to statement
 * @param stmt  - the statement
 * @param narg  - argument number for binding
 * @param str   - the text itself
 * @return 0 if all OK
 */
static int addtext(sqlite3_stmt *stmt, int narg, const char *str){
    if(str == NULL) str = "";
    int rc = sqlite3_bind_text(stmt, narg, str, -1, SQLITE_TRANSIENT);
    if(rc != SQLITE_OK){
        WARNX("Can't bind %s: %d", str, rc);
        return 1;
    }
    return 0;
}


/**
 * @brief addWSkey - add new websocket ID to session & send this ID to user
 * @param res - web response
 * @param session - session information to modify
 */
void addWSkey(onion_response *res, sessinfo *session){
    if(!res || !session) return;
    do{
        FREE(session->sockID);
        session->sockID = onion_sessions_generate_id();
        DBG("Try to modify session %s", session->sessID);
        if(!addSession(session, 1)){
            DBG("Modify session: ID=%s, sockid=%s, atime=%" PRId64 ", user=%s, data=%s\n",
            session->sessID, session->sockID, session->atime, session->username, session->data);
            break;
        }
    }while(1);
    onion_response_write0(res, session->sockID);
}

/**
 * @brief qookieSession - find session ID from SESSION_COOKIE_NAME and search session in DB
 * @param req - web request
 * @return session data (allocated here) or NULL if session not found
 */
sessinfo *qookieSession(onion_request *req){
    if(!req) return NULL;
    /*const char *path = onion_request_get_path(req);
    printf("Ask path %s\n", path);*/
    onion_dict *cookies = onion_request_get_cookies_dict(req);
    const char *skey = onion_dict_get(cookies, SESSION_COOKIE_NAME);
    if(!skey){
        WARNX("No session cookie found\n");
        return NULL;
    }
    return getSession(skey);
}

onion_connection_status auth(_U_ onion_handler *h, onion_request *req, onion_response *res){
    if(!req || !res) return OCS_CLOSE_CONNECTION;
    if(onion_request_get_flags(req) & OR_HEAD) {
        onion_response_write_headers(res);
        return OCS_PROCESSED;
    }
    const char *host = onion_request_get_client_description(req);
    char json[2048]; // json buffer for UA & IP
    const char *UA = onion_request_get_header(req, "User-Agent");
    snprintf(json, 2048, "{\"User-Agent\": \"%s\", \"User-IP\": \"%s\"}", UA, host);
    DBG("Client: %s, UA: %s\n", host, UA);
    const char *logout = getQdata(req, "LogOut");
    DBG("logout=%s\n", logout);
    sessinfo *session = qookieSession(req);
    if(!session) DBG("No cookie, need to create\n");
    else if(!logout){
        onion_response_write0(res, AUTH_ANS_AUTHOK);
        goto closeconn;
    }
    const char *username = NULL, *passwd = NULL;
    if(logout){
        DBG("User logged out\n");
        if(session){
            if(deleteSession(session->sessID))
                WARNX("Can't delete session with ID=%s from database", session->sessID);
        }
        onion_response_write0(res, AUTH_ANS_LOGOUT);
        onion_response_add_cookie(res, SESSION_COOKIE_NAME, "clear", 0, "/", NULL, OC_HTTP_ONLY|OC_SECURE);
        goto closeconn;
    }else{ // log in
        freeSessInfo(&session);
        username = getQdata(req, "login");
        if(!username){
            ONION_WARNING("no login field -> need auth");
            onion_response_write0(res, AUTH_ANS_NEEDAUTH);
            return OCS_CLOSE_CONNECTION;
        }
        passwd = getQdata(req, "passwd");
        if(!passwd){
            ONION_WARNING("Trying to enter authenticated area without password");
            onion_response_write0(res, AUTH_ANS_NOPASSWD);
            return OCS_FORBIDDEN;
        }
    }
    userinfo *U = getUserData(username);
    if(!U){
        WARNX("User %s not found", username);
        return OCS_FORBIDDEN;
    }
    char *pass = strdup(crypt(passwd, "$6$"));
    if(!pass){
        WARN("Error in ctypt or strdup");
        freeUserInfo(&U);
        return OCS_FORBIDDEN;
    }
    int comp = strcmp(pass, U->password);
    freeUserInfo(&U);
    FREE(pass);
    if(comp){
        WARNX("User %s give wrong password", username);
        return OCS_FORBIDDEN;
    }
    session = MALLOC(sessinfo, 1);
    session->atime = time(NULL);
    session->username = strdup(username);
/*
    onion_dict *data = onion_dict_new();
    onion_dict_add(data, "UA", UA, 0);
    onion_dict_add(data, "IP", host, 0);
    onion_block *bl = onion_dict_to_json(data);
    if(bl){
        const char *json = onion_block_data(bl);
        if(json) session->data = strdup(json);
    }*/
    session->data = strdup(json);
    do{
        FREE(session->sessID);
        session->sessID = onion_sessions_generate_id();
        DBG("Try to add session %s", session->sessID);
        if(!addSession(session, 0)){
            DBG("New session: ID=%s, atime=%" PRId64 ", user=%s, data=%s\n",
            session->sessID, session->atime, session->username, session->data);
            break;
        }
        sleep(2);
    }while(1);
    onion_response_add_cookie(res, SESSION_COOKIE_NAME, session->sessID, 366*86400, "/", NULL, OC_HTTP_ONLY|OC_SECURE);
    onion_response_write0(res, AUTH_ANS_AUTHOK);
closeconn:
    freeSessInfo(&session);
    return OCS_CLOSE_CONNECTION;
}

/**
 * @brief opendb - open SQLite database
 * @param name - database filename
 * @param db   - database itself
 * @return 0 if all OK
 */
static int opendb(const char *name, sqlite3 **db){
    if(!name || !db) return 2;
    if(SQLITE_OK != sqlite3_open(name, db)){
        ONION_ERROR("Can't open database: %s", sqlite3_errmsg(*db));
        sqlite3_close(*db);
        return 1;
    }
    return 0;
}

/**
 * @brief close_sessDB - close session database & free memory
 */
static void close_sessDB(){
    if(!sessionDB) return;
    pthread_mutex_lock(&sessionDB->mutex);
    sqlite3_finalize(sessionDB->add);
    sqlite3_finalize(sessionDB->del);
    sqlite3_finalize(sessionDB->getsid);
    sqlite3_finalize(sessionDB->getsockid);
    sqlite3_close(sessionDB->db);
    pthread_mutex_unlock(&sessionDB->mutex);
    pthread_mutex_destroy(&sessionDB->mutex);
    FREE(sessionDB->name);
    FREE(sessionDB);
}
/**
 * @brief close_authDB - close user database & free memory
 */
static void close_authDB(){
    if(!authDB) return;
    pthread_mutex_lock(&authDB->mutex);
    sqlite3_finalize(authDB->add);
    sqlite3_finalize(authDB->del);
    sqlite3_finalize(authDB->get);
    sqlite3_close(authDB->db);
    pthread_mutex_unlock(&authDB->mutex);
    pthread_mutex_destroy(&authDB->mutex);
    FREE(authDB->name);
    FREE(authDB);
}

/**
 * @brief closeSQLite - close all databases
 */
void closeSQLite(){
    close_authDB();
    close_sessDB();
}

/**
 * @brief initSQLite    - init SQL databases
 * @param auth_filename - database with auth data (username, password hash, access level)
 * @param sess_filename - session database (session ID, websosket ID, access time, data {UA, IP, username, access level - JSON})
 * @return error code or 0 if OK
 */
int initSQLite(const char *auth_filename, const char *sess_filename){
    if(!auth_filename || !sess_filename) return 5;
    int rc;
    char *zErrMsg = NULL;
    onion_random_init();
    closeSQLite();
    /******************************* users database *******************************/
    authDB = MALLOC(_authDB, 1);
    if(opendb(auth_filename, &authDB->db)) return 2;
    authDB->name = strdup(auth_filename);
    rc = sqlite3_exec(authDB->db, "CREATE TABLE passwd (user TEXT PRIMARY KEY NOT NULL, passhash TEXT, level INT, comment TEXT)", NULL, NULL, &zErrMsg);
    if(rc != SQLITE_OK){
        if(strcmp(zErrMsg, "table passwd already exists")){
            WARNX("SQL CREATE error %d: %s", rc, zErrMsg);
            goto ret;
        }
    }else{
        fprintf(stderr, "Add default user\n");
        // default admin: toor, password: p@ssw0rd
        rc = sqlite3_exec(authDB->db, "INSERT INTO passwd VALUES (\"toor\", \"$6$$F55h0JlnNG19zS6YHUDX6h4zksblEBCFRqzCoCt6Y3VqE7zBeM/ifdGbUNMK.dLIwu9jq3fCmLBYEpzEpNCej0\", 0, \"\")", NULL, NULL, &zErrMsg);
        if(rc != SQLITE_OK){
            fprintf(stderr, "SQL INSERT error %d: %s\n", rc, zErrMsg);
            sqlite3_free(zErrMsg);
            goto ret;
        }
    }
    // get user information
    if(sqprep(authDB->db, "SELECT passhash, level, comment FROM passwd WHERE user=?",
              &authDB->get)) goto ret;
    // add or modify user
    if(sqprep(authDB->db, "INSERT OR REPLACE INTO passwd (user, passhash, level, comment) VALUES (?, ?, ?, ?)",
              &authDB->add)) goto ret;
    // delete user
    if(sqprep(authDB->db, "DELETE FROM passwd WHERE user=?", &authDB->del)) goto ret;
    if(pthread_mutex_init(&authDB->mutex, NULL)){
        WARN("Can't init authDB mutex");
        goto ret;
    }
    /****************************** session database ******************************/
    sessionDB = MALLOC(_sessionDB, 1);
    if(opendb(sess_filename, &sessionDB->db)) goto ret;
    sessionDB->name = strdup(sess_filename);
    rc = sqlite3_exec(sessionDB->db, "CREATE TABLE sessions (sessID TEXT PRIMARY KEY NOT NULL, sockID TEXT, atime INT, username TEXT, data TEXT)", NULL, NULL, &zErrMsg);
    if(rc != SQLITE_OK){
        if(strcmp(zErrMsg, "table sessions already exists")){
            WARNX("SQL CREATE error %d: %s", rc, zErrMsg);
            sqlite3_free(zErrMsg);
            goto ret;
        }
    }
    // get session information
    if(sqprep(sessionDB->db, "SELECT sockID, atime, username, data FROM sessions WHERE sessID=?",
              &sessionDB->getsid)) goto ret;
    if(sqprep(sessionDB->db, "SELECT sessID, atime, username, data FROM sessions WHERE sockID=?",
              &sessionDB->getsockid)) goto ret;
    // add or modify session
    if(sqprep(sessionDB->db, "INSERT OR REPLACE INTO sessions (sessID, sockID, atime, username, data) VALUES (?, ?, ?, ?, ?)",
              &sessionDB->add)) goto ret;
    // delete session
    if(sqprep(sessionDB->db, "DELETE FROM sessions WHERE sessID=?",
              &sessionDB->del)) goto ret;
    if(sqprep(sessionDB->db, "DELETE FROM sessions WHERE atime<?",
              &sessionDB->delold)) goto ret;
    return 0;
ret:
    closeSQLite();
    return -1;
}

/**
 * @brief getUserData - find all records in authDB for given username
 * @param username - user name
 * @return userinfo structure allocated here or NULL if user not found
 */
userinfo *getUserData(const char *username){
    if(!username) return NULL;
    userinfo *U = NULL;
    if(!authDB){
        WARNX("User database not initialized");
        return NULL;
    }
    pthread_mutex_lock(&authDB->mutex);
    sqlite3_reset(authDB->get);
    if(addtext(authDB->get, 1, username)) return NULL;
    if(SQLITE_ROW != sqlite3_step(authDB->get)){
        WARNX("User %s not found", username);
        goto ret;
    }
    const char *pass = (const char *) sqlite3_column_text(authDB->get, 0);
    int levl = sqlite3_column_int(authDB->get, 1);
    const char *comment = (const char *) sqlite3_column_text(authDB->get, 2);
    U = (userinfo *) malloc(sizeof(userinfo));
    if(!U) goto ret;
    U->username = strdup(username);
    U->password = strdup(pass);
    U->level = levl;
    U->comment = strdup(comment);
ret:
    pthread_mutex_unlock(&authDB->mutex);
    return U;
}

/**
 * @brief freeUserInfo - free memory for struct userinfo
 * @param x - pointer to userinfo pointer
 */
void freeUserInfo(userinfo **x){
    if(!x || !*x) return;
    userinfo *U = *x;
    free(U->username);
    free(U->password);
    free(U->comment);
    free(U);
    *x = NULL;
}

// callback for showAllUsers()
static int selallcb(_U_ void *notused, int argc, char **argv, _U_ char **no){
    if(argc != 4){
        fprintf(stderr, "Wrong argc: %d\n", argc);
        return 1;
    }
    printf("%s\t%s\t%s\t%s\n", argv[0], argv[2], argv[1], argv[3]);
    return 0;
}
/**
 * @brief showAllUsers - printout user database
 */
void showAllUsers(){
    char *zErrMsg = NULL;
    green("\nUSER\tLevel\tPassHash\t\t\tComment\n");
    pthread_mutex_lock(&authDB->mutex);
    int rc = sqlite3_exec(authDB->db, "SELECT * FROM passwd GROUP BY user", selallcb, NULL, &zErrMsg);
    if(rc != SQLITE_OK){
        WARNX("SQL CREATE error %d: %s", rc, zErrMsg);
        sqlite3_free(zErrMsg);
    }
    pthread_mutex_unlock(&authDB->mutex);
}

/**
 * @brief deleteUser - delete user record from database
 * @param username - user name
 * @return 0 if all OK
 */
int deleteUser(const char *username){
    if(!username) return 1;
    userinfo *u = getUserData(username);
    if(!u) return 1;
    FREE(u);
    pthread_mutex_lock(&authDB->mutex);
    sqlite3_reset(authDB->del);
    if(addtext(authDB->del, 1, username)) goto reterr;
    if(SQLITE_DONE != sqlite3_step(authDB->del)) goto reterr;
    else{
        printf("User %s deleted\n", username);
        pthread_mutex_unlock(&authDB->mutex);
        return 0;
    }
reterr:
    pthread_mutex_unlock(&authDB->mutex);
    WARNX("Can't delete user %s", username);
    return 2;
}

/**
 * @brief addUser - add user to database
 * @param User - struct with user data
 * @return 0 if OK
 */
int addUser(userinfo *User){
    if(!User) return 1;
    if(strlen(User->username) < 1){
        WARNX("No username");
        return 1;
    }
    pthread_mutex_lock(&authDB->mutex);
    sqlite3_reset(authDB->add);
    if(addtext(authDB->add, 1, User->username)) goto reterr;
    if(addtext(authDB->add, 2, User->password)) goto reterr;
    if(SQLITE_OK != sqlite3_bind_int(authDB->add, 3, User->level)) goto reterr;
    addtext(authDB->add, 4, User->comment);
    if(SQLITE_DONE == sqlite3_step(authDB->add)){
        green("Add to database:\n");
        printf("\tuser %s, passHash %s, level %d, comment '%s'\n",
               User->username, User->password, User->level, User->comment);
        pthread_mutex_unlock(&authDB->mutex);
        return 0;
    }
reterr:
    pthread_mutex_unlock(&authDB->mutex);
    WARNX("Can't insert user %s", User->username);
    return 1;
}

/**
 * @brief getSession - find session by session ID or socket ID
 * @param ID - session or socket ID
 * @return NULL if not found or session structure (allocated here)
 */
sessinfo *getSession(const char *ID){
    if(!ID) return NULL;
    sqlite3_stmt *stmt = sessionDB->getsid;
    sessinfo *s = NULL;
    pthread_mutex_lock(&sessionDB->mutex);
    sqlite3_reset(stmt);
    if(addtext(stmt, 1, ID)) goto ret;
    const char *sessID = NULL, *sockID = NULL;
    if(SQLITE_ROW != sqlite3_step(stmt)){
        stmt = sessionDB->getsockid;
        sqlite3_reset(stmt);
        if(addtext(stmt, 1, ID)) goto ret;
        if(SQLITE_ROW != sqlite3_step(stmt)){
            WARNX("Session %s not found\n", ID);
            goto ret;
        }else{
            sockID = ID;
            sessID = (const char *) sqlite3_column_text(stmt, 0);
        }
    }else{
        sessID = ID;
        sockID = (const char *) sqlite3_column_text(stmt, 0);
    }
    // 0-ID, 1-atime, 2-username, 3-data
    int64_t atime = sqlite3_column_int64(stmt, 1);
    const char *username = (const char *)sqlite3_column_text(stmt, 2);
    const char *data = (const char *)sqlite3_column_text(stmt, 3);
    s = (sessinfo*) malloc(sizeof(sessinfo));
    if(!s) goto ret;
    s->sessID = strdup(sessID);
    s->sockID = strdup(sockID);
    s->atime = atime;
    s->username = strdup(username);
    s->data = strdup(data);
ret:
    pthread_mutex_unlock(&sessionDB->mutex);
    return s;
}

/**
 * @brief addSession - add new session or modify old
 * @param s - structure with session information
 * @param modify != 0 to modify existant session
 * @return 0 if all OK
 */
int addSession(sessinfo *s, int modify){
    if(!s) return 1;
    sessinfo *byID = getSession(s->sessID);
    sessinfo *bysID = getSession(s->sockID);
    if(byID || bysID){ // found session with same IDs
        if(modify){
            if(bysID && strcmp(bysID->sessID, s->sessID)){
                freeSessInfo(&byID);
                freeSessInfo(&bysID);
                WARNX("Found another session with the same sockID");
                return 2;
            }
        }else{
            if(byID) WARNX("Found another session with the same sessID");
            if(bysID) WARNX("Found another session with the same sockID");
            freeSessInfo(&byID);
            freeSessInfo(&bysID);
            return 3;
        }
    }
    pthread_mutex_lock(&sessionDB->mutex);
    // 1-sessID, 2-sockID, 3-atime, 4-username, 5-data
    sqlite3_reset(sessionDB->add);
    if(addtext(sessionDB->add, 1, s->sessID)) goto reterr;
    if(addtext(sessionDB->add, 2, s->sockID)) goto reterr;
    if(SQLITE_OK != sqlite3_bind_int64(sessionDB->add, 3, s->atime)) goto reterr;
    if(addtext(sessionDB->add, 4, s->username)) goto reterr;
    if(addtext(sessionDB->add, 5, s->data)) goto reterr;
    if(SQLITE_DONE == sqlite3_step(sessionDB->add)){
        pthread_mutex_unlock(&sessionDB->mutex);
        return 0;
    }
reterr:
    pthread_mutex_unlock(&sessionDB->mutex);
    WARNX("Can't insert session %s\n", s->sessID);
    return 1;
}

/**
 * @brief deleteSession - delete session by SessID
 * @param sessID - session ID
 * @return 0 if all OK
 */
int deleteSession(const char *sessID){
    if(!sessID) return 1;
    pthread_mutex_lock(&sessionDB->mutex);
    sqlite3_reset(sessionDB->del);
    if(addtext(sessionDB->del, 1, sessID)) goto reterr;
    if(SQLITE_DONE != sqlite3_step(sessionDB->del)) goto reterr;
    else{
        printf("Session with id %s deleted\n", sessID);
        pthread_mutex_unlock(&sessionDB->mutex);
        return 0;
    }
reterr:
    pthread_mutex_unlock(&sessionDB->mutex);
    WARNX("Can't delete session with ID %s\n", sessID);
    return 1;
}

/**
 * @brief deleteSession - delete session by SessID
 * @param minatime - minimal access time (all sessions with atime<minatime will be deleted)
 * @return 0 if all OK
 */
int deleteOldSessions(int64_t minatime){
    pthread_mutex_lock(&sessionDB->mutex);
    sqlite3_reset(sessionDB->delold);
    if(SQLITE_OK != sqlite3_bind_int64(sessionDB->delold, 1, minatime)) goto reterr;
    if(SQLITE_DONE != sqlite3_step(sessionDB->delold)) goto reterr;
    else{
        pthread_mutex_unlock(&sessionDB->mutex);
        return 0;
    }
reterr:
    pthread_mutex_unlock(&sessionDB->mutex);
    WARNX("Can't delete sessions with atime < %" PRId64 " \n", minatime);
    return 1;
}

// callback for showAllSessions
static int selallscb(void _U_ *notused, int argc, char **argv, char _U_ **no){
    if(argc != 5){
        fprintf(stderr, "Wrong argc: %d\n", argc);
        return 1;
    }
    printf("%s\t%s\t%s\t%s\t%s\n", argv[3], argv[2], argv[0], argv[1], argv[4]);
    return 0;
}
void showAllSessions(){
    char *zErrMsg = NULL;
    green("Username\tAtime\tSession ID\t\tSocket ID\t\tData\n");
    int rc = sqlite3_exec(sessionDB->db, "SELECT * FROM sessions ORDER BY username", selallscb, NULL, &zErrMsg);
    if(rc != SQLITE_OK){
        WARNX("SQL CREATE error %d: %s\n", rc, zErrMsg);
        sqlite3_free(zErrMsg);
    }
}

/**
 * @brief freeSessInfo - free session info structure
 * @param si - address of pointer to SI
 */
void freeSessInfo(sessinfo **si){
    if(!si || !*si) return;
    sessinfo *s = *si;
    free(s->data);
    free(s->sessID);
    free(s->sockID);
    free(s->username);
    free(*si);
    *si = NULL;
}

/**
 * @brief vacuum - rebuild databases to free all deleted records
 */
void vacuumSQLite(){
    char *zErrMsg = NULL;
    int rc = sqlite3_exec(authDB->db, "vacuum", NULL, NULL, &zErrMsg);
    if(rc != SQLITE_OK){
        WARNX("Error in vacuum %s (%d): %s\n", authDB->name, rc, zErrMsg);
        sqlite3_free(zErrMsg);
    }
    rc = sqlite3_exec(sessionDB->db, "vacuum", NULL, NULL, &zErrMsg);
    if(rc != SQLITE_OK){
        WARNX("Error in vacuum %s (%d): %s\n", sessionDB->name, rc, zErrMsg);
        sqlite3_free(zErrMsg);
    }
    green("Both databases are rebuilt\n");
}

