#include "data.h"
#include <cstdlib>
#include <map>
#include <vector>
#include <filesystem>
#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/sendfile.h>

#ifndef GUISERVER_H
#define GUISERVER_H

#define BUFSIZE 2048
#define MAX_NAME 128
#define MAX_TIME 600
#define QUEUE 10
#define HTML_PW_ID "skey"
#define HTML_TXT_ID "entry"

namespace Files {
    const std::string dirPath = std::filesystem::current_path().string() + "/";
    const std::vector<std::string> htmlFiles = {"index.html", "mainpage.html"};
}

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
        void updateState(int newState) { this->m_state = newState; }
        bool isAuthenticated() const;
        ~Session() {}
};

const std::string makeHeader(int );
int handleRequest(char *, std::shared_ptr<Session>, sqlite3* );
int runServer(sqlite3* );

#endif
