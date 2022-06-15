/*
** server.c -- a stream socket server demo
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>

// added packets
#include <ostream>
#include <iostream>
#include <fstream>
#include <sstream>

#define PORT "3490"  // the port users will be connecting to

#define MAXDATASIZE 200
#define BACKLOG 10	 // how many pending connections queue will hold

void sigchld_handler(int s)
{
	while(waitpid(-1, NULL, WNOHANG) > 0);
}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

std::string readFileIntoString(const char * filename)
{
	std::ifstream ifile(filename);
	std::ostringstream buf;
	char ch;
	while(buf&&ifile.get(ch))
		buf.put(ch);
	return buf.str();
}



int main(int argc, char *argv[])	// only a port is needed
{
	if (argc != 2) {
	    fprintf(stderr,"usage: ./http_server port_number\n");
	    exit(1);
	}
	char* port = argv[1];
	std::string port_str = port;
	int port_int = atoi(port_str.c_str());
	if ((port_int < 0) | (port_int > 65535)) {
	    fprintf(stderr,"port out of range\n");
	    exit(1);
	}

	int sockfd, new_fd;  // listen on sock_fd, new connection on new_fd
	struct addrinfo hints, *servinfo, *p;
	struct sockaddr_storage their_addr; // connector's address information
	socklen_t sin_size;
	struct sigaction sa;
	int yes=1;
	char s[INET6_ADDRSTRLEN];
	int rv;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE; // use my IP

	if ((rv = getaddrinfo(NULL, port, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// loop through all the results and bind to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("server: socket");
			continue;
		}

		if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
				sizeof(int)) == -1) {
			perror("setsockopt");
			exit(1);
		}

		if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("server: bind");
			continue;
		}

		break;
	}

	if (p == NULL)  {
		fprintf(stderr, "server: failed to bind\n");
		return 2;
	}

	freeaddrinfo(servinfo); // all done with this structure

	if (listen(sockfd, BACKLOG) == -1) {
		perror("listen");
		exit(1);
	}

	sa.sa_handler = sigchld_handler; // reap all dead processes
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	if (sigaction(SIGCHLD, &sa, NULL) == -1) {
		perror("sigaction");
		exit(1);
	}

	printf("server: waiting for connections...\n");

	while(1) {  // main accept() loop
		sin_size = sizeof their_addr;
		new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
		if (new_fd == -1) {
			perror("accept");
			continue;
		}

		inet_ntop(their_addr.ss_family,
			get_in_addr((struct sockaddr *)&their_addr),
			s, sizeof s);
		printf("server: got connection from %s\n", s);

		if (!fork()) { // this is the child process
			close(sockfd); // child doesn't need the listener

            // receiving http request
			int rcvsize;
            char buff[MAXDATASIZE];
			memset(buff, 0, MAXDATASIZE);
            std::cout << "Receiving HTTP response from " << s << std::endl;
            if ((rcvsize = recv(new_fd, buff, MAXDATASIZE, 0)) == -1) {
                perror("Receive Error\n");
                exit(1);
            } else if (rcvsize == 0) {
                std::cout << "Connection Closed\n";
                exit(1);
            } else {
				std::cout << "Received message:" << std::endl;
                std::cout << buff << std::endl;
            } 

			// parse the request to get the file name
            std::string file = buff;
            int pos = file.find("\r");
            file = file.substr(0, pos);
            file = file.substr(5, file.length() - 14);
            std::cout << std::endl << "Requested file:" << std::endl;
            std::cout << file << std::endl;
			if (file.find(" ") != std::string::npos)
			{
				if (send(new_fd, "HTTP/1.1 400 Bad Request\r\n", 26, 0) == -1)
					perror("send 400 bad request error");
			}

			// open the file and return the file content
			FILE* fp = fopen(file.c_str(), "r");
			int exist = 1;
			if (fp == NULL)
			{
				exist = 0;
				if (send(new_fd, "HTTP/1.1 404 Not Found\r\n", 24, 0) == -1)
					perror("send 404 not found error");
			}
			fclose(fp);
			std::string file_content;
			if (exist == 1)
			{
				file_content = readFileIntoString(file.c_str());
			}
			// std::cout << file_content << std::endl;
			if (send(new_fd, "HTTP/1.1 200 OK\r\n\r\n", 19, 0) == -1)
					perror("sending content error");
			if (send(new_fd, file_content.c_str(), file_content.length(), 0) == -1)
					perror("sending content error");

			close(new_fd);
			exit(0);
		}
		close(new_fd);  // parent doesn't need this
	}

	return 0;
}

