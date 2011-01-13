#include "comm.h"
#include "rumble.h"

extern masterHandle* master;
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

socketHandle comm_init(const char* port)
{
	int sockfd;  // our socket! yaaay.
	struct addrinfo hints, *servinfo, *p;
	int yes=1;
	int rv;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = rumble_config_int("forceipv4") ? AF_INET : AF_UNSPEC; // Force IPv4 or use default?
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE; // use my IP

	if ((rv = getaddrinfo(NULL, port, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 0;
	}

	// Loop through all the results and bind to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("server: socket");
			continue;
		}

		if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
				sizeof(int)) == -1) {
			perror("setsockopt");
			exit(0);
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
		return 0;
	}

	freeaddrinfo(servinfo); // all done with this structure

	if (listen(sockfd, 10) == -1) {
		perror("listen");
		exit(0);
	}
        return sockfd;
}

void comm_accept(socketHandle sock, clientHandle* client) {
    socklen_t sin_size = sizeof client->client_info;
    while(1) {  // loop through accept() till we get something worth passing along.
        client->socket = accept(sock, (struct sockaddr *)&(client->client_info), &sin_size);
        if (client->socket == -1) {
                perror("Error while attempting accept()");
                break;
        }

        inet_ntop(client->client_info.ss_family,
                get_in_addr((struct sockaddr *)&client->client_info),
                client->addr, sizeof client->addr);
        break;
    }
}

void comm_mxLookup(const char* domain)
{ 
// wtb a proper unix machine to develop on...oh wait, I got one...somewhere...
    
 }
