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
#define BUFSIZE 2048
#define PWBUFSIZE 256
#define MINPWLEN 8
#define ENCTYPE sodium_base64_VARIANT_ORIGINAL

int sqlExecute(sqlite3 * , const std::string, bool );
bool setPassword(sqlite3 * , std::string );
bool resetPassword(sqlite3 * , std::string );
int checkPassword(sqlite3 * , std::string );
int decodeAndDecrypt(unsigned char* , std::string , std::string , const size_t );
int saveEntry(sqlite3 * , std::string , std::string );
int printEntries(sqlite3 * , std::string );
int getSalt(sqlite3 * , char * );
std::string deriveKey(sqlite3 * , std::string , bool=false );

#endif