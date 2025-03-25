#include <string>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <sodium.h>
#include <sqlite3.h>

#ifndef DATA_H
#define DATA_H

#define DBFILE "pages.db"

int sqlExecute(sqlite3 *, std::string);
bool setPassword(sqlite3 *, std::string );

#endif