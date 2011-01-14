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
	struct addrinfo hints;
	int yes=1;

        memset(&hints, 0, sizeof hints);
	hints.ai_family = rumble_config_int("forceipv4") ? AF_INET : AF_UNSPEC; // Force IPv4 or use default?
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE; // use my IP
        printf("socket stuff...\n");
        #ifdef RUMBLE_WINSOCK
                WSADATA wsaData; 
                if (WSAStartup(MAKEWORD(2, 0), &wsaData) != 0) { perror("Winsock failed to start"); exit(EXIT_FAILURE); }
                if ((sockfd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) <= 0) { perror("Winsock: Couldn't create socket"); }
                if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (char*) &yes,
				sizeof(int)) == SOCKET_ERROR) {
			perror("setsockopt");
			exit(0);
		} 
                struct sockaddr_in x;
                x.sin_family = hints.ai_family;
                x.sin_port = htons(atoi(port));
                x.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
                
		if (bind(sockfd, (struct sockaddr*)&x, sizeof(x)) == SOCKET_ERROR) {
			close(sockfd);
                        fprintf(stderr,"Server: failed to bind: %d\n", WSAGetLastError());
                        exit(0);
		}
                if (&x == NULL)  {
                        fprintf(stderr, "server: failed to bind\n");
                        exit(0);
                }
        printf ("listening...\n");
        #else
	int rv;
        struct addrinfo* servinfo, *p;
	if ((rv = getaddrinfo(NULL, port, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 0;
	}

	// Loop through all the results and bind to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == SOCKET_ERROR) {
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
        #endif
	if (listen(sockfd, 10) == SOCKET_ERROR) {
		perror("listen");
		exit(0);
	}
        
        return sockfd;
}

void comm_accept(socketHandle sock, clientHandle* client) {
    socklen_t sin_size = sizeof client->client_info;
    while(1) {  // loop through accept() till we get something worth passing along.
        client->socket = accept(sock, (struct sockaddr *)&(client->client_info), &sin_size);
        if (client->socket == SOCKET_ERROR) {
                perror("Error while attempting accept()");
                break;
        }

#ifdef RUMBLE_WINSOCK
        strncpy(client->addr, inet_ntoa( ((struct sockaddr_in *)&(client->client_info))->sin_addr ), 46);
#else
        inet_ntop(client->client_info.ss_family,
                get_in_addr((struct sockaddr *)&client->client_info),
                client->addr, sizeof client->addr);
#endif
        break;
    }
}


cvector* comm_mxLookup(const char* domain)
{ 
    cvector* vec = cvector_init();
    
#ifdef RUMBLE_WINSOCK // Windows MX resolver
    DNS_STATUS status;
    struct _DnsRecord* rec;
    status = DnsQuery_A(domain, DNS_TYPE_MX, DNS_QUERY_STANDARD, 0, &rec,0);
    if ( !status ) {
        while ( rec ) {
            if ( rec->wType == DNS_TYPE_MX ) {
                mxRecord* mx = malloc(sizeof(mxRecord));
                ssize_t len = strlen(rec->Data.MX.pNameExchange);
                mx->host = calloc(1,len+1);
                strncpy((char*) mx->host, rec->Data.MX.pNameExchange, len);
                mx->preference = rec->Data.MX.wPreference;
                cvector_add(vec, mx);
            }
            rec = rec->pNext;
        }
        DnsRecordListFree(rec, DNS_TYPE_MX);
    }
    
   
#else // UNIX (IBM) MX resolver
  u_char nsbuf[4096];
  memset(nsbuf, 0, sizeof(nsbuf));
  int  l;
  ns_msg query_parse_msg;
  ns_rr query_parse_rr;

  // Try to resolve domain
  res_init();
  l = res_search (domain, ns_c_in, ns_t_mx, nsbuf, sizeof (nsbuf));
  if (l < 0) { // Resolving failed
    return NULL;
  }
  ns_initparse(nsbuf, l, &query_parse_msg);
  int x;
  for (x = 0; x < ns_msg_count(query_parse_msg, ns_s_an); x++)
       {
         if (ns_parserr(&query_parse_msg, ns_s_an, x, &query_parse_rr))
         {
             break;
         }
         if (ns_rr_type(query_parse_rr) == ns_t_mx)
 	  {
             mxRecord* mx = malloc(sizeof(mxRecord));
             mx->preference = ns_get16((u_char *)ns_rr_rdata(query_parse_rr));
             mx->host = calloc(1,1024);
             if (ns_name_uncompress(ns_msg_base(query_parse_msg), ns_msg_end(query_parse_msg), (u_char  *)ns_rr_rdata(query_parse_rr)+2, (char*) mx->host, 1024) < 0) break;
             cvector_add(vec, mx);
 	  }
     }
#endif
    // Fall back to A record if no MX exists
    if ( cvector_size(vec) == 0 ) {
        struct hostent* a = gethostbyname(domain);
        if ( a ) {
            mxRecord* mx = malloc(sizeof(mxRecord));
            struct in_addr x;
            bcopy(a->h_addr_list++, &x, sizeof(x));
            char* b = inet_ntoa(x);
            ssize_t len = strlen(b);
            mx->host = calloc(1,len+1);
            strncpy((char*) mx->host, b, len);
            mx->preference = 10;
            free(a);
        }
    }
    return vec;
 }
