#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/select.h>

// Global structures to manage clients and FDs
typedef struct s_client {
    int     id;
    char    *msg;
} t_client;

t_client    clients[1024]; // Max FDs is usually 1024
fd_set      read_set, write_set, active_set;
int         max_fd = 0;
int         next_id = 0;
char        send_buffer[200000]; // Buffer for formatting messages

// --- Provided Helper Functions ---

int extract_message(char **buf, char **msg)
{
    char    *newbuf;
    int     i;

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
    char    *newbuf;
    int     len;

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

// --- Custom Helper Functions ---

void fatal_error() {
    write(2, "Fatal error\n", 12);
    exit(1);
}

void send_all(int except_fd, char *str) {
    for (int fd = 0; fd <= max_fd; fd++) {
        // Send to everyone connected (in write_set) except the sender
        if (FD_ISSET(fd, &write_set) && fd != except_fd) {
            send(fd, str, strlen(str), 0);
        }
    }
}

int main(int ac, char **av) {
    int sockfd, connfd;
    struct sockaddr_in servaddr, cli; 
    socklen_t len;

    if (ac != 2) {
        write(2, "Wrong number of arguments\n", 26);
        exit(1);
    }

    // socket create and verification 
    sockfd = socket(AF_INET, SOCK_STREAM, 0); 
    if (sockfd == -1) 
        fatal_error(); 
    
    bzero(&servaddr, sizeof(servaddr)); 

    // assign IP, PORT 
    servaddr.sin_family = AF_INET; 
    servaddr.sin_addr.s_addr = htonl(2130706433); // 127.0.0.1
    servaddr.sin_port = htons(atoi(av[1])); 
  
    // Binding newly created socket to given IP and verification 
    if ((bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr))) != 0) 
        fatal_error(); 
  
    if (listen(sockfd, 10) != 0)
        fatal_error();

    // Initialize FD sets
    FD_ZERO(&active_set);
    FD_SET(sockfd, &active_set);
    max_fd = sockfd;

    while (1) {
        // Reset sets for select
        read_set = active_set;
        write_set = active_set;

        // Block until something happens
        if (select(max_fd + 1, &read_set, &write_set, NULL, NULL) < 0)
            continue;

        for (int fd = 0; fd <= max_fd; fd++) {
            if (FD_ISSET(fd, &read_set)) {
                
                // Case 1: New Connection
                if (fd == sockfd) {
                    len = sizeof(cli);
                    connfd = accept(sockfd, (struct sockaddr *)&cli, &len);
                    if (connfd >= 0) {
                        if (connfd > max_fd) max_fd = connfd;
                        
                        clients[connfd].id = next_id++;
                        clients[connfd].msg = NULL; // Init buffer
                        FD_SET(connfd, &active_set);

                        sprintf(send_buffer, "server: client %d just arrived\n", clients[connfd].id);
                        send_all(connfd, send_buffer);
                    }
                } 
                // Case 2: Data from client
                else {
                    char recv_buffer[4096];
                    int bytes = recv(fd, recv_buffer, 4094, 0);

                    // Client Disconnected
                    if (bytes <= 0) {
                        sprintf(send_buffer, "server: client %d just left\n", clients[fd].id);
                        send_all(fd, send_buffer);
                        FD_CLR(fd, &active_set);
                        close(fd);
                        if (clients[fd].msg) free(clients[fd].msg);
                    } 
                    // Message Received
                    else {
                        recv_buffer[bytes] = 0;
                        clients[fd].msg = str_join(clients[fd].msg, recv_buffer);
                        
                        char *msg_line = NULL;
                        while (extract_message(&clients[fd].msg, &msg_line)) {
                            sprintf(send_buffer, "client %d: ", clients[fd].id);
                            send_all(fd, send_buffer);
                            send_all(fd, msg_line);
                            free(msg_line);
                        }
                    }
                }
            }
        }
    }
    return (0);
}