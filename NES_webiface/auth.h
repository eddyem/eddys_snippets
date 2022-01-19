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

#pragma once
#ifndef AUTH_H__
#define AUTH_H__

#include <onion/onion.h>

#define SESSION_COOKIE_NAME     "Acookie"

// standard answers to client
#define AUTH_ANS_NEEDAUTH   "NeedAuth"
#define AUTH_ANS_AUTHOK     "AuthOK"
#define AUTH_ANS_LOGOUT     "LogOut"
#define AUTH_ANS_NOPASSWD   "NoPassword"

typedef struct{
    char *username;     // user name
    char *password;     // password hash (SHA512)
    int level;          // user access level
    char *comment;      // optional comment
} userinfo;

typedef struct{
    char *sessID;       // session ID
    char *sockID;       // websoket ID (cleared when disconnected or after 1 minute after `atime`)
    int64_t atime;      // last access time (UNIX time) - all ID data cleared after 1yr of inactivity
    char *username;     // username
    char *data;         // JSON data with UA, IP etc
} sessinfo;

const char *getQdata(onion_request *req, const char *key);
onion_connection_status auth(onion_handler *h, onion_request *req, onion_response *res);
void addWSkey(onion_response *res, sessinfo *session);

int initSQLite(const char *auth_filename, const char *sess_filename);
void closeSQLite();
void vacuumSQLite();

userinfo *getUserData(const char *username);
void freeUserInfo(userinfo **x);
void showAllUsers();
int deleteUser(const char *username);
int addUser(userinfo *User);

sessinfo *getSession(const char *ID);
sessinfo *qookieSession(onion_request *req);
int addSession(sessinfo *s, int modify);
int deleteSession(const char *sessID);
int deleteOldSessions(int64_t minatime);
void showAllSessions();
void freeSessInfo(sessinfo **si);

#endif // AUTH_H__
