#include "data.h"
#include <cstdlib>
#include <map>
#include <vector>
#include <filesystem>
#include <fcntl.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/sendfile.h>

#ifndef GUISERVER_H
#define GUISERVER_H

#define MAX_NAME 128
#define MAX_TIME 600
#define MIN_ENTRY 5
#define QUEUE 10
#define HIDE_PW_SPEED 50000
#define HTML_PW_ID "skey"
#define HTML_TXT_ID "entry"

namespace Files {
    const std::string dirPath = std::filesystem::current_path().string() + "/";
    const std::vector<std::string> htmlFiles = {"index.html", "mainpage.html", "saved.html"};
    const int maxState = htmlFiles.size() - 1;
}

class Session {
    private:
        std::uint64_t m_id;
        std::time_t m_start;
        int m_state;
        int m_feed;
        std::string m_key;
    public:
        Session(std::uint64_t userId, int clientFd);
        int getState() const { return m_state; }
        int getFeed() const { return m_feed; }
        std::string getKey() const { return m_key; }
        void updateState(int newState) { this->m_state = newState; }
        bool isAuthenticated() const;
        int setKey(sqlite3 * db, std::string pw);
        ~Session() {}
};

const std::string makeHeader(int );
int handleRequest(char *, std::shared_ptr<Session>, sqlite3* );
int runServer(sqlite3* );
void testFunction();

#endif
