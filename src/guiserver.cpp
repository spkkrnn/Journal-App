#include <fcntl.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <unistd.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/sendfile.h>

#define BUFSIZE 1024
#define NAME_MAX 128
#define QUEUE 10
#define HTML_FILE "index.html"

void construct_header(char *header, char *filename) {
	char* path = malloc(NAME_MAX * sizeof(char));
    path = std::filesystem::currentpath();
	snprintf(header, BUFSIZE, "HTTP/1 200 OK\r\nContent-Type: text/html\r\nContent-Length: %ld\r\n\r\n", filesize);
}


int runServer(void) {
    unsigned short port = 8989;
    unsigned short max_port = port + 10; // try 10 times to bind port
    char buffer[BUFSIZE] = {0};

	char header[256];
	memset(header, 0, 256);
    int file, client_fd;
    int state = 1;

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
    printf("Socket created.\n");

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
    setsockopt(sock, IPPROTO_TCP, TCP_CORK, &state, sizeof(state));

    while (1) {
        // listen for connections
        if (listen(sock, QUEUE) < 0) {
		    perror("listening error");
            close(sock);
		    return -1;
	    }
        printf("Listening for connections...\n");

        // receive connection
        if ((client_fd = accept(sock, 0, 0)) < 0) {
            perror("accepting error");
            close(sock);
            return -1;
        }
        if (recv(client_fd, buffer, BUFSIZE, 0) < 0) {
            perror("receiving error");
            close(sock);
            close(client_fd);
            return -1;
        }

        // process the request
        char *file_id = buffer + 5;
        *strchr(file_id, ' ') = 0;

        // check file size and open file
        long filesize;
        if ((file = open(filename, O_RDONLY)) < 0) { 
            perror("file error");
            close(client_fd);
            close(sock);
            return -1;
        }

        construct_header(header, filename, filesize);
        // send header and then file
        if (write(client_fd, header, strlen(header)) < 0) {
            perror("writing error");
            close(file);
            close(client_fd);
            close(sock);
            return -1;
        }
        off_t offset = 0;
        while ((sent = sendfile(client_fd, file, &offset, BUFSIZE)) > 0){
            if (sent < 0) {
	        perror("sending error");
	        close(file);
	        close(client_fd);
	        close(sock);
      	    return -1;
	    }
        }
    }
    // close everything
    close(file);
    close(client_fd);
    client_fd = -1;
    close(sock);

    return 0;
}
