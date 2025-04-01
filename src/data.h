#include <string>
#include <ctime>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <sodium.h>
#include <sqlite3.h>

#ifndef DATA_H
#define DATA_H

#define DBFILE "pages.db"
#define PWBUFSIZE 256
#define MINPWLEN 8

int sqlExecute(sqlite3 *, std::string, bool );
bool setPassword(sqlite3 *, std::string );
int checkPassword(sqlite3 *, std::string );

#endif