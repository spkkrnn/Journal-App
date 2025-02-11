#ifndef GUISERVER_H
#define GUISERVER_H

#define BUFSIZE 1024
#define NAME_MAX 128
#define QUEUE 10

void makeHeader(int );
int handleRequest(char *, int &, int &)
int runServer();

#endif
