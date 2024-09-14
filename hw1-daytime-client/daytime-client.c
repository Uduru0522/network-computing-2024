#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define DAYTIME_PORT 13
#define BUF_SIZE 1024

int main(int argc, char *argv[])
{
	if(argc != 2){
		fprintf(stderr, "Usage: %s <ip-address>\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	char *dest = argv[1], buf[BUF_SIZE];
	struct sockaddr *server_addr; // Pointer to in or in6 struct
	int addr_size;

	// Determine ip address version
	struct sockaddr_in addr4;
	struct sockaddr_in6 addr6;
	if(inet_pton(AF_INET, dest, &(addr4.sin_addr)) == 1){
		addr4.sin_family = AF_INET;
		addr4.sin_port = htons(DAYTIME_PORT);
		server_addr = (struct sockaddr *)&addr4;
		addr_size = sizeof(struct sockaddr_in);
	}
	else if(inet_pton(AF_INET6, dest, &(addr6.sin6_addr)) == 1){
		addr6.sin6_family = AF_INET6;
		addr6.sin6_port = htons(DAYTIME_PORT);
		server_addr = (struct sockaddr *)&addr6;
		addr_size = sizeof(struct sockaddr_in6);	
	}
	else{
		fprintf(stderr, "Error: Invaliad IP address (ipv4 or ipv6)\n");	
		exit(EXIT_FAILURE);
	}

	// Create socket
	int s = socket(server_addr->sa_family, SOCK_STREAM, 0);
	if(s == -1){
		fprintf(stderr, "Error: Failed creating socket");
		exit(EXIT_FAILURE);
	}
	
	// Attempt connecting to server
	if(connect(s, server_addr, addr_size) == -1){
		fprintf(stderr, "Erroe: Failed connecting to server (%s)\n", dest);
		perror("connect");
		exit(EXIT_FAILURE);
	}

	// Get server ip object
	memset(buf, 0, BUF_SIZE);
	int recv_len = recv(s, buf, BUF_SIZE, 0);
       	if(recv_len < 0){
		fprintf(stderr, "Error: recv message failed\n");
		exit(EXIT_FAILURE);
	}	
	printf("%s\n", buf);

	close(s);
	return 0;
}
