#include <string>

#ifndef GUISERVER_H
#define GUISERVER_H

#define BUFSIZE 1024
#define NAME_MAX 128
#define MAX_TIME 600
#define QUEUE 10

const std::string makeHeader(int );
int handleRequest(char *, int &, int &);
int runServer();

#endif
