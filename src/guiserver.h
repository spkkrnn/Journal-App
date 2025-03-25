#include <ctime>
#include <cstdlib>
#include <string>
#include <cstdio>
#include <cstring>
#include <csignal>
#include <map>
#include <vector>
#include <iostream>
#include <filesystem>
#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/sendfile.h>

#ifndef GUISERVER_H
#define GUISERVER_H

#define BUFSIZE 1024
#define MAX_NAME 128
#define MAX_TIME 600
#define QUEUE 10

class Session {
    private:
        std::uint64_t m_id;
        std::time_t m_start;
        int m_state;
        int m_feed;
    public:
        Session(std::uint64_t userId, int clientFd);
        int getState() const { return m_state; }
        int getFeed() const { return m_feed; }
        void updateState() { this->m_state++; }
        bool isAuthenticated() const;
        ~Session() {}
};

const std::string makeHeader(int );
int handleRequest(char *, std::shared_ptr<Session> );
int runServer();

#endif
