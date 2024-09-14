#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <arpa/inet.h>
#include <netdb.h>

#define PORT "22654"
#define BUF_SIZE 1024
#define BACKLOG 16
#define RESPONSE "HTTP/1.0 200 OK\r\n" \
	"Content-Type: text/html\r\n" \
	"\r\n" \
	"<h1>Hello, World!</h1>\r\n" \
	"<p>apple apple <font color=red>apple</font> apple apple</p>"

void handle_client(int client_fd) {
    char buffer[BUF_SIZE];
    int bytes_received = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
    if (bytes_received < 0) {
        perror("recv");
    } else {
        buffer[bytes_received] = '\0';
        printf("Request:\n%s\n", buffer);
        send(client_fd, RESPONSE, strlen(RESPONSE), 0);
    }
    close(client_fd);
}

void start_server(int sockfd) {
    struct sockaddr_storage their_addr;
    socklen_t addr_size = sizeof(their_addr);

    while (1) {
        int new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &addr_size);
        if (new_fd == -1) {
            perror("accept");
            continue;
        }

        if (fork() == 0) {
            close(sockfd); 
            handle_client(new_fd);
            exit(0);
        }
    }
}

int main() {
    struct addrinfo server_info_hint, *server_info_list;
    int sockfd, rv;

    memset(&server_info_hint, 0, sizeof(server_info_hint));
    server_info_hint.ai_family = AF_UNSPEC; // Both IPv4 and IPv6
    server_info_hint.ai_socktype = SOCK_STREAM;
    server_info_hint.ai_flags = AI_PASSIVE;

    if ((rv = getaddrinfo(NULL, PORT, &server_info_hint, &server_info_list)) != 0) {
        fprintf(stderr, "getaddrinfo() failed: %s\n", gai_strerror(rv));
        return 1;
    }

    struct addrinfo *p;
    for (p = server_info_list; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("Socket creating failed, trying next...");
            continue;
        }

        int yes = 1;
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
            perror("setsockopt failed, trying next...");
            close(sockfd);
            exit(-1);
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            perror("Socket binding failed, trying next...");
            close(sockfd);
            continue;
        }

        break;
    }

    freeaddrinfo(server_info_list);

    if (p == NULL) {
        fprintf(stderr, "Binding to all possible serverinfo failed\n");
        exit(-1);
    }

    if (listen(sockfd, BACKLOG) == -1) {
        perror("Failed to start listening to socket");
        exit(-1);
    }

    printf("Server listening on port %s\n", PORT);
    
    // Infinite loop at here
    start_server(sockfd);

    close(sockfd);
    return 0;
}
