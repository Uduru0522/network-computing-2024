#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>

#define BUFFER_SIZE 4096
#define MAX_RETRY 3
#define RETRY_DELAY 5

void error(const char *msg)
{
    fprintf(stderr, "%s\n", msg);
    exit(1);
}

int main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <FileURL>\n", argv[0]);
        return 1;
    }

    // Parse URL
    char *url = argv[1];
    char protocol[8], host[256], path[1024] = {0};
    int port = 80;

    if (sscanf(url, "%9[^:]://[%255[^]]]:%d%1023[^\n]", protocol, host, &port, path) != 4){
        if (sscanf(url, "%9[^:]://[%255[^]]]%1023[^\n]", protocol, host, path) != 3){
            if (sscanf(url, "%9[^:]://%255[^:]:%d%1023[^\n]", protocol, host, &port, path) != 4){
                if (sscanf(url, "%9[^:]://%255[^/]%1023[^\n]", protocol, host, path) != 3){
                    error("unsupported url format, how is it even possible to get here?");
                }
            }
        }
    }

    // Resolve hostname using getaddrinfo
    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    char port_str[6];
    snprintf(port_str, sizeof(port_str), "%d", port);

    if (getaddrinfo(host, port_str, &hints, &res) != 0){
        fprintf(stderr, "Error: Could not resolve host\n");
        return 1;
    }

    int sockfd, retry = 0;
    while (retry < MAX_RETRY){
        printf("Attempting to connect to server...");

        // Create socket
        sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (sockfd < 0){
            printf("Error opening socket, retrying in 5 seconds\n");
            retry++;
            sleep(RETRY_DELAY);
            continue;
        }

        // Connect to server
        if (connect(sockfd, res->ai_addr, res->ai_addrlen) < 0){
            printf("Error connecting, retrying in 5 seconds\n");
            close(sockfd);
            retry++;
            sleep(RETRY_DELAY);
            continue;
        }

        printf("Success!\n");
        break;
    }

    freeaddrinfo(res);

    if (retry == MAX_RETRY){
        fprintf(stderr, "Failed to connect after %d attempts\n", MAX_RETRY);
        return 1;
    }

    // Send HTTP GET request
    char request[2048] = {0};
    sprintf(request, "GET %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n", path, host);
    if (write(sockfd, request, strlen(request)) < 0){
        error("Error writing to socket");
    }

    char buffer[BUFFER_SIZE];
    int bytes_read, total_bytes = 0;

    // Receive first response, trim the header
    if(bytes_read = read(sockfd, buffer, BUFFER_SIZE) < 0){
        error("Error reading frm socket");
        goto cleanup;
    }
    total_bytes += bytes_read;

    // Requested non-existing file, got 404
    if(strstr(buffer, "404")){
        error("404 File not found");
    }

    // Add suffix to filename if already exists, lets build the _new train :))
    char *filename = strrchr(path, '/') + 1;
    while(access(filename, F_OK) == 0){
        int len = strlen(filename);
        const char *suffix = "_new";
        char ext[16] = {0}, *cur = strrchr(path, '.');
        strncpy(ext, cur, strlen(cur));
        strncpy(cur, suffix, strlen(suffix));
        strncpy(cur + strlen(suffix), ext, strlen(ext));
        printf("%s, %s\n", path, filename);
    }

    // Open file to write response
    FILE *file = fopen(filename, "wb");
    if (file == NULL){
        error("Error opening file");
    }

    char *cur = strstr(buffer, "\r\n\r\n") + 4;
    fwrite(cur, 1, bytes_read - (cur - buffer), file);

    // Receive additional response and save to file
    while ((bytes_read = read(sockfd, buffer, BUFFER_SIZE)) > 0){
        fwrite(buffer, 1, bytes_read, file);
        total_bytes += bytes_read;
    }

    if (bytes_read < 0){
        error("Error reading from socket");
    }

    printf("Received file '%s'\n", filename);

cleanup: 
    close(sockfd);
    fclose(file);

    printf("File downloaded successfully.\n");
    return 0;
}
