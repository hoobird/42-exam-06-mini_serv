#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>

typedef struct s_client {
	int id;
	char *buffer;
} t_client;

t_client clients[1024];

int max_fd=0;
int next_id= 0;
char sendbuffer[20000];
fd_set activeset, readset, writeset;


int extract_message(char **buf, char **msg)
{
	char	*newbuf;
	int	i;

	*msg = 0;
	if (*buf == 0)
		return (0);
	i = 0;
	while ((*buf)[i])
	{
		if ((*buf)[i] == '\n')
		{
			newbuf = calloc(1, sizeof(*newbuf) * (strlen(*buf + i + 1) + 1));
			if (newbuf == 0)
				return (-1);
			strcpy(newbuf, *buf + i + 1);
			*msg = *buf;
			(*msg)[i + 1] = 0;
			*buf = newbuf;
			return (1);
		}
		i++;
	}
	return (0);
}

char *str_join(char *buf, char *add)
{
	char	*newbuf;
	int		len;

	if (buf == 0)
		len = 0;
	else
		len = strlen(buf);
	newbuf = malloc(sizeof(*newbuf) * (len + strlen(add) + 1));
	if (newbuf == 0)
		return (0);
	newbuf[0] = 0;
	if (buf != 0)
		strcat(newbuf, buf);
	free(buf);
	strcat(newbuf, add);
	return (newbuf);
}

void fatal_error() {
	write(2, "Fatal error\n", 13);
	exit(1);
}

void send_all(int sender, char *msg) {
	for (int i = 0; i <= max_fd; ++i) {
		if (FD_ISSET(i, &writeset) && i != sender) {
			send(i, msg, strlen(msg), 0);
		}
	}
}

int main(int argc, char** argv) {
	int sockfd, connfd, len;
	struct sockaddr_in servaddr, cli; 

	if (argc != 2) {
		write(2, "Wrong number of arguments\n",26);
		return 1;
	}

	// socket create and verification 
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd == -1) { 
		fatal_error(); 
	} 
	bzero(&servaddr, sizeof(servaddr)); 

	// assign IP, PORT 
	servaddr.sin_family = AF_INET; 
	servaddr.sin_addr.s_addr = htonl(2130706433); //127.0.0.1
	servaddr.sin_port = htons(atoi(argv[1]));
  
	// Binding newly created socket to given IP and verification 
	if ((bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr))) != 0) { 
		fatal_error(); 
	} 
	if (listen(sockfd, 10) != 0) { 
		fatal_error(); 
	} 

	FD_ZERO(&activeset);
	FD_SET(sockfd, &activeset);;
	max_fd = sockfd;

	while (1) {
		readset = activeset;
		writeset = activeset;

		if (select(max_fd + 1, &readset, &writeset, NULL, 0) < 0) {
			continue;
		}

		for (int i = 0; i <= max_fd; ++i) {
			if (FD_ISSET(i, &readset)) {
				// new connection
				if (i == sockfd) {
					len = sizeof(cli);
					int cfd = accept(sockfd, (struct sockaddr *)&cli, &len);
					if (cfd >= 0) {
						if (cfd > max_fd) {
							max_fd = cfd;
						}
						FD_SET(cfd, &activeset);
						clients[cfd].id = next_id++;
						clients[cfd].buffer = NULL;
						sprintf(sendbuffer, "server: client %d just arrived\n", clients[cfd].id);
						send_all(cfd, sendbuffer);
					}
				}
				else {
					char recvbuffer[4096];
					int recvbytes = recv(i, recvbuffer, 4094, 0);
					
					if (recvbytes <= 0) {
						sprintf(sendbuffer, "server: client %d just left\n", clients[i].id);
						send_all(i, sendbuffer);
						FD_CLR(i, &activeset);
						close(i);
						if (clients[i].buffer) {
							free(clients[i].buffer);
							clients[i].buffer = 0;
						}
					}else {
						recvbuffer[recvbytes] = 0;
						clients[i].buffer = str_join(clients[i].buffer, recvbuffer);
						
						char *msg_line = NULL;
						while (extract_message(&clients[i].buffer, &msg_line)){
							sprintf(sendbuffer, "client %d: ", clients[i].id);
							send_all(i,sendbuffer);
							send_all(i,msg_line);
							free(msg_line);
						}
					}
				}
				
			}
		}

	}

	
	// len = sizeof(cli);
	// connfd = accept(sockfd, (struct sockaddr *)&cli, &len);
	// if (connfd < 0) { 
	// 	printf("server acccept failed...\n"); 
	// 	exit(0); 
	// } 
	// else
	// 	printf("server acccept the client...\n");
}

// "Fatal error\n"
// "Wrong number of arguments\n"
// "server: client %d just arrived\n"
// "server: client %d just left\n"
// "client %d: %s"