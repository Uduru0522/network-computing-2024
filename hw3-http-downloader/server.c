#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define BUFFER_SIZE 4096
#define PORT 8080
#define BACKLOG 10

void error(const char *msg)
{
    perror(msg);
    exit(1);
}

void serve_file(int client_fd, const char *filepath)
{
    printf("Attempting to send file: '%s'\n", filepath);
    int file_fd = open(filepath, O_RDONLY);
    if (file_fd < 0){
        const char *not_found = "HTTP/1.1 404 Not Found\r\nContent-Length: 13\r\n\r\n404 Not Found";
        write(client_fd, not_found, strlen(not_found));
        return;
    }

    struct stat file_stat;
    fstat(file_fd, &file_stat);
    int file_size = file_stat.st_size;

    char header[BUFFER_SIZE];
    sprintf(header, "HTTP/1.1 200 OK\r\nContent-Length: %d\r\n\r\n", file_size);
    write(client_fd, header, strlen(header));

    char buffer[BUFFER_SIZE];
    int bytes_read;
    while ((bytes_read = read(file_fd, buffer, BUFFER_SIZE)) > 0){
        write(client_fd, buffer, bytes_read);
    }

    close(file_fd);
}

void handle_client(int client_fd)
{
    char buffer[BUFFER_SIZE];
    int bytes_read = read(client_fd, buffer, BUFFER_SIZE - 1);
    if (bytes_read < 0){
        error("Error reading from socket");
    }
    buffer[bytes_read] = '\0';

    char method[16], path[256], version[16];
    path[0] = '.';
    sscanf(buffer, "%s %s %s", method, path + 1, version);

    if (strcmp(method, "GET") != 0){
        const char *not_allowed = "HTTP/1.1 405 Method Not Allowed\r\nContent-Length: 19\r\n\r\n405 Method Not Allowed";
        write(client_fd, not_allowed, strlen(not_allowed));
    } 
    else{
        serve_file(client_fd, path);
    }

    close(client_fd);
}

void start_server(int sockfd)
{
    struct sockaddr_storage their_addr;
    socklen_t addr_size = sizeof(their_addr);

    while (1) {
        int new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &addr_size);
        if (new_fd == -1){
            perror("accept");
            continue;
        }

        if (fork() == 0){
            close(sockfd);
            handle_client(new_fd);
            exit(0);
        }
        close(new_fd);
    }
}

int main() 
{
    struct addrinfo hints, *res, *p;
    int sockfd, rv;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET6;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if ((rv = getaddrinfo(NULL, PORT, &hints, &res)) != 0){
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // Setup socket so accept both ipv4 and ipv6 address
    for (p = res; p != NULL; p = p->ai_next){
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1){
            perror("socket");
            continue;
        }

        int yes = 1;
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1){
            perror("setsockopt");
            close(sockfd);
            return 1;
        }

        int no = 0;
        if (setsockopt(sockfd, IPPROTO_IPV6, IPV6_V6ONLY, &no, sizeof(no)) == -1){
            perror("setsockopt");
            close(sockfd);
            return 1;
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1){
            perror("bind");
            close(sockfd);
            continue;
        }

        break;
    }

    freeaddrinfo(res);

    if (p == NULL){
        fprintf(stderr, "failed to bind\n");
        return 1;
    }

    if (listen(sockfd, BACKLOG) == -1){
        perror("listen");
        return 1;
    }

    printf("Server listening on port %s\n", PORT);

    start_server(sockfd);

    close(sockfd);
    return 0;
}
