#include "guiserver.h"

inline Session::Session(std::uint64_t userId, int clientFd) // constructor
    : m_id{ userId }
    , m_start{ std::time(nullptr) }
    , m_state{ 0 }
    , m_feed{ clientFd }
    , m_key{ {0} }
{
}

inline bool Session::isAuthenticated() const {
    if (m_state == 0) {
        return false;
    }
    std::time_t sessionDuration = std::time(nullptr) - m_start;
    if (sessionDuration > MAX_TIME) {
        std::cout << "Session has timed out with duration " << sessionDuration << std::endl;
        return false;
    }
    return true;
}

inline int Session::setKey(sqlite3* db, std::string pw) {
    std::string key = deriveKey(db, pw);
    this->m_key = key;
    return 0;
}

std::uint64_t formUserId(int address) { // NOTE: Port number is unusable since browsers change port every request
    std::uint32_t shift32 = -1;
    std::uint64_t userId = (std::uint64_t) address * (std::uint64_t) shift32; // renewed method
    userId += (std::uint64_t) address;
    return userId;
}

std::size_t getFileSize(int fileId) {
    std::string fullPath = Files::dirPath + Files::htmlFiles[fileId];
    std::size_t fileSize = std::filesystem::file_size(fullPath);
    return fileSize;
}

std::shared_ptr<Session> addSession(std::uint64_t id, int clientFd, std::map<std::uint64_t, std::shared_ptr<Session>> &userList) {
     if (userList.empty() || userList.count(id) == 0) {
        std::shared_ptr<Session> newSession = std::make_shared<Session>(id, clientFd);
        userList.insert(std::pair<std::uint64_t, std::shared_ptr<Session>>(id, newSession));
     }
     return userList[id];
}

void popWhitespaces(std::string& text) {
    if (text.length() < 1) {
        return;
    } 
    char c = text.back();
    if (c == '\n' || c == '\r' || c == ' ') {
        text.pop_back();
        popWhitespaces(text);
    }
}

const std::string makeHeader(int fileId) {
    std::size_t fileSize = getFileSize(fileId);
	const std::string header = "HTTP/1.0 200 OK\r\nConnection: Close\r\nContent-Type: text/html\r\nContent-Length: " + std::to_string(fileSize) + "\r\n\r\n";
    return header;
}

const std::string getPayload(char* request) {
    char* ptr = request;
    int i;
    std::uint8_t lineChanges = 0; 
    for (i = 0; i < BUFSIZE; ++i) {
        if (*ptr == '\0') {
            return "";
        }
        else if (*ptr == '\r' || *ptr == '\n') {
            lineChanges++;
        }
        else {
            lineChanges = 0;
        }
        ptr++;
        if (lineChanges == 4) {
            break;
        }
    }
    const std::size_t contentLen = strnlen(ptr, (BUFSIZE - i - 1));
    const std::string payload(ptr, contentLen);
    return payload;
}

int sendHeader(std::string header, int clientFd) {
    if (write(clientFd, header.c_str(), header.length()) < 0) {
        perror("writing error");
        close(clientFd);
        return -1;
    }
    std::cout << "Header sent." << std::endl; // test print
    return 0;
}

int sendResponse(int fileId, int clientFd) {
    int file;
    if ((file = open(Files::htmlFiles[fileId].c_str(), O_RDONLY)) < 0) { 
        perror("file error");
        close(clientFd);
        return -1;
    }
    const std::string header = makeHeader(fileId);
    if (sendHeader(header, clientFd) < 0) {
        close(file);
        return -1;
    }
    off_t offset = 0;
    int sent = 0;
    while ((sent = sendfile(clientFd, file, &offset, BUFSIZE)) > 0){
        if (sent < 0) {
            perror("sending error");
            close(file);
            close(clientFd);
            return -1;
        }
    }
    close(file);
    std::cout << "Response sent." << std::endl; // test print
    return 0;
}

int handleRequest(char* request, std::shared_ptr<Session> clientSession, sqlite3* db) {
    int clientState = clientSession->getState();
    int clientFeed = clientSession->getFeed();
    std::string pw;
    if (*request == 'G') { // GET request
        sendResponse(clientState, clientFeed);
        return 0;
    }
    else if (*request == 'P') { // POST request
        const std::string post = getPayload(request);
        // test print
        std::cout << post << std::endl;
        size_t divPos = post.find("=");
        if (divPos < 0 || (divPos >= post.length())) {
            return -1;
        }
        std::string postContent = post.substr(divPos + 1); //TODO: sanitize input
        popWhitespaces(postContent);
        if (post.find(HTML_PW_ID) == 0) {
            if (checkPassword(db, postContent) < 1) {
                std::string header = "HTTP/1.1 403 Forbidden\r\nConnection: Close\r\nContent-Type: text/plain\r\nContent-Length: 15\r\n\r\n403 Forbidden\r\n";
                sendHeader(header, clientFeed);
                clientSession->updateState(0);
                return 0;
            }
            pw = postContent;
            clientSession->updateState(1);
        }
        else if (post.find(HTML_TXT_ID) == 0) {
            if (!clientSession->isAuthenticated()) {
                std::string header = "HTTP/1.1 401 Unauthorized\r\nConnection: Close\r\nContent-Type: text/plain\r\nContent-Length: 53\r\n\r\n403 Unauthorized\r\nNot logged in or session timed out.\r\n";
                sendHeader(header, clientFeed);
                clientSession->updateState(0);
                return 0;
            }
            else if (postContent.length() > MIN_ENTRY) {
                std::string key = clientSession->getKey();
                int newState = (saveEntry(db, postContent, key) < 0) ? 1 : 2;
                clientSession->updateState(newState);
            }
        }
        clientState = clientSession->getState();
        sendResponse(clientState, clientFeed);
        if (!pw.empty()) {
            clientSession->setKey(db, pw);
        }
        return 0;
    }
    return -1;
}

int runServer(sqlite3* db) {
    unsigned short port = 8989;
    unsigned short max_port = port + 10; // try 10 times to bind port

    int client_fd;
    char* buffer = (char*) calloc(BUFSIZE, sizeof(char));
    std::map<std::uint64_t, std::shared_ptr<Session>> userList;

    // set up socket
    int sock;
    struct sockaddr_in addr;
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation error");
        return -1;
    }
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
    std::cout << "Socket created." << std::endl;

    // bind sock to addr, change port number if fails
	while (port < max_port) {
        if (bind(sock, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
		    perror("binding error");
            addr.sin_port = htons(++port);
            continue;
	    }
        break;
    }
    // exit program if all binds fail
    if (port >= max_port) {
        std::cerr << "Bind unsuccessful." << std::endl;
        close(sock);
        return -1;
    }
    std::cout << "Socket bound to port " << port << std::endl;

    while (1) {
        // listen for connections
        if (listen(sock, QUEUE) < 0) {
		    perror("listening error");
            close(sock);
		    return -1;
	    }
        std::cout << "Listening for connections..." << std::endl;

        // receive connection
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        if ((client_fd = accept(sock, (struct sockaddr *) &client_addr, &client_len)) < 0) {
            perror("accepting error");
            close(sock);
            return -1;
        }
        //inet_ntop(AF_INET, &client_addr->sin_addr.s_addr, str_addr, INET_ADDRSTRLEN);
        //std::cout << "Connected from " << str_addr << std::endl;
        if (recv(client_fd, buffer, BUFSIZE, 0) < 0) {
            perror("receiving error");
            close(sock);
            close(client_fd);
            return -1;
        }
        std::cout << "Connection from port " << client_addr.sin_port << std::endl;
        std::uint64_t id = formUserId(client_addr.sin_addr.s_addr);
        std::shared_ptr<Session> currentSession = addSession(id, client_fd, userList);
        if (userList.size() > QUEUE) {
            std::cout << "Too many clients." << std::endl;
            break;
        }
        handleRequest(buffer, currentSession, db);
        // null the buffer and close client feed
        memset(buffer, 0, BUFSIZE);
        close(client_fd);
        client_fd = -1;
        if (currentSession->getState() == 2) {
            break;
        }
    }
    // close everything
    free(buffer);
    close(sock);

    return 0;
}
