#include "guiserver.h"

namespace Files {
    const std::string dirPath = std::filesystem::current_path().string() + "/";
    const std::vector<std::string> htmlFiles = {"index.html", "mainpage.html"};
}

inline Session::Session(std::uint64_t userId, int clientFd) // constructor
    : m_id{ userId }
    , m_start{ std::time(nullptr) }
    , m_state{ 0 }
    , m_feed{ clientFd }
{
}

inline bool Session::isAuthenticated() const {
    if (m_state == 0) {
        return false;
    }
    std::time_t sessionDuration = std::time(nullptr) - m_start;
    if (sessionDuration > MAX_TIME) {
        return false;
    }
    return true;
}

std::uint64_t formUserId(int address, unsigned short port) {
    std::uint32_t shift32 = -1;
    std::uint64_t userId = (std::uint64_t) port * (std::uint64_t) shift32;
    userId += (std::uint64_t) address;
    return userId;
}

std::size_t getFileSize(int fileId) {
    std::string fullPath = Files::dirPath + Files::htmlFiles[fileId];
    std::size_t fileSize = std::filesystem::file_size(fullPath);
    return fileSize;
}

Session* addSession(std::uint64_t id, int clientFd, std::map<std::uint64_t, Session*>* userList) {
     if (userList->count(id) == 0) {
        userList->at(id) = new Session(id, clientFd);
     }
     return userList->at(id);
}

const std::string makeHeader(int fileId) {
    std::size_t fileSize = getFileSize(fileId);
	const std::string header = "HTTP/1.0 200 OK\r\nContent-Type: text/html\r\nContent-Length: " + std::to_string(fileSize) + "\r\n\r\n";
    return header;
}

int sendResponse(int fileId, int clientFd) {
    int file;
    if ((file = open(Files::htmlFiles[fileId].c_str(), O_RDONLY)) < 0) { 
        perror("file error");
        close(clientFd);
        //close(sock);
        return -1;
    }
    const std::string header = makeHeader(fileId);
    if (write(clientFd, header.c_str(), header.length()) < 0) {
        perror("writing error");
        close(file);
        close(clientFd);
        //close(sock);
        return -1;
    }
    off_t offset = 0;
    int sent = 0;
    while ((sent = sendfile(clientFd, file, &offset, BUFSIZE)) > 0){
        if (sent < 0) {
            perror("sending error");
            close(file);
            close(clientFd);
            //close(sock);
            return -1;
        }
    }
    close(file);
    return 0;
}

int handleRequest(char* request, Session* clientSession) {
    int clientState = clientSession->getState();
    int clientFeed = clientSession->getFeed();
    if (request[0] == 'G') { // GET request
        sendResponse(clientState, clientFeed);
        return 0;
    }
    else if (request[0] == 'P') { // POST request
        return 0;
    }
    return -1;
}

int runServer(void) {
    unsigned short port = 8989;
    unsigned short max_port = port + 10; // try 10 times to bind port

    int client_fd;
    //int state = 1;
    std::map<std::uint64_t, Session*> * userList = new std::map<uint64_t, Session*>;
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
    //setsockopt(sock, IPPROTO_TCP, TCP_CORK, &state, sizeof(state));

    while (1) {
        // listen for connections
        if (listen(sock, QUEUE) < 0) {
		    perror("listening error");
            close(sock);
		    return -1;
	    }
        std::cout << "Listening for connections..." << std::endl;

        // receive connection
        struct sockaddr_in* client_addr;
        char* buffer = (char*) calloc(BUFSIZE, sizeof(char));
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
        std::cout << "Connection from port " << client_addr->sin_port << std::endl;
        std::uint64_t id = formUserId(client_addr->sin_addr.s_addr, client_addr->sin_port);
        Session* currentSession = addSession(id, client_fd, userList);
        if (userList->size() > QUEUE) {
            std::cout << "Too many clients." << std::endl;
            break;
        }
        handleRequest(buffer, currentSession);
        free(buffer);
    }
    // close everything
    //close(file);
    close(client_fd);
    client_fd = -1;
    close(sock);

    return 0;
}
