/*
** client.c -- a stream socket client demo
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>

// added packets
#include <ostream>
#include <iostream>
#include <fstream>
using namespace::std;

#define PORT "3490" // the port client will be connecting to 

#define MAXDATASIZE 1024// max number of bytes we can get at once 

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}



/*
 *	Modified to receive HTTP
 *
 * 
 * 
 */
int main(int argc, char *argv[])
{
	int sockfd;  
	// char buf[MAXDATASIZE];
	struct addrinfo hints, *servinfo, *p;
	int rv;
	char s[INET6_ADDRSTRLEN];
	string URL;		// url to access, in form like "http://xxxx..." 

	if (argc != 2) {
	    fprintf(stderr,"usage: client hostname\n");
	    exit(1);
	}

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	// split the URL into host + path
	size_t pos = 0;
	string str_http = "http://";
	string host, path;
	URL = argv[1];
	if ((pos = URL.find(str_http)) == string::npos) {
		cout << "Format Error without 'http://'" << endl;
		exit(1);
	} else {
		URL.erase(0, pos + str_http.length());	// erase useless string
	}

	size_t tmp_pos;					// the first '/' in url
	if ((tmp_pos = URL.find("/")) == string::npos) {
		cout << "No File specified" << endl;
		exit(1);
	} else {
		host = URL.substr(0, tmp_pos);
		path = URL.substr(tmp_pos);
	}

	// further split host into hostname + port if port is specified
	string hostname, port;
	if ((pos = host.find(":")) == string::npos) {
		hostname = host;
		port = "80"; 		// default http port
	} else {
		hostname = host.substr(0, pos);
		port = host.substr(pos+1);
	}
	// debug check
	cout << str_http << host << path << endl;
	cout << hostname << ":" << port << endl;







	if ((rv = getaddrinfo(hostname.data(), port.data(), &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// loop through all the results and connect to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("client: socket");
			continue;
		}

		if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("client: connect");
			continue;
		}

		break;
	}

	if (p == NULL) {
		fprintf(stderr, "client: failed to connect\n");
		return 2;
	}

	inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
			s, sizeof s);
	printf("client: connecting to %s\n", s);

	freeaddrinfo(servinfo); // all done with this structure

	// a connection is established between client and http
	// now sent HTTP request to the server http
	string req_msg = "GET " + path + " HTTP/1.1\r\n" + 
					 "User-agent: Wget/1.12 (linux-gnu)\r\n" + 
					 "Host: " + hostname + ":" + port + "\r\n" + 
					 "Connection: Keep-Alive\r\n\r\n";
	int send_counter = 0;
	// send HTTP request and keep trying until success or fail more than 10 times
	size_t sendsize;
	cout << "\nHTTP request:\n" << req_msg << endl;
	cout << "\nSending HTTP request to " << s << endl;
	while ((int)(sendsize = send(sockfd, req_msg.c_str(), req_msg.size(), 0)) == -1 || (sendsize != req_msg.size())) {
		cout << "Resend HTTP Request " << send_counter << endl;
		if (++send_counter >= 10) {
			cout << "Send HTTP Request Fail: Time Out." << endl;
			exit(1);
		} continue;
	} cout << "Sending Succeeds" << endl;

	// receive HTTP response and store the reponse into output.txt
	int rcvsize = 0, line = 0, file_size = 0;
	char buff[MAXDATASIZE];
	memset(buff, 0, MAXDATASIZE);
	cout << "Receiving HTTP response from " << s << endl;
	cout << "Start Receiving..." << endl;

	ofstream OUTPUT("output");
	if (!OUTPUT.is_open()) {
		cout << "Open output file failed\n";
		exit(1);
	}
	while (1) {
		memset(buff, 0, MAXDATASIZE);
		rcvsize = recv(sockfd, buff, MAXDATASIZE-1, 0);
		int start = 0;
		line++;
		if (rcvsize == -1) {
			perror("Receive Error\n");
			exit(1);
		} else if (rcvsize == 0) {
			cout << "Connection Closed: Receive Done.\n";
			break;
		} else {
			cout << buff << endl;
			file_size += rcvsize;
			if (line == 1) {
				for (int i = 0; i < rcvsize; i++) 
					if (buff[i] == '\r' && buff [i+1] == '\n' && buff[i+2] == '\r' && buff [i+3] == '\n')	
						start = i + 4;
				if (start == 0)	break;
			}
			for (int i = start; i < rcvsize; i++) 
			 	OUTPUT << buff[i];
		} 
	} cout << "Receiving Succeeds: " << "Total " << file_size << " Bytes received." << endl;

	OUTPUT.close();
	close(sockfd);

	return 0;
}

