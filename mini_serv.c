#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h> // Required for malloc/free/exit
#include <stdio.h>  // Required for sprintf

// --- GLOBALS (Added to manage state) ---
int     clients[65536]; // To store client IDs (index is the FD)
char    *msg_buffer[65536]; // To store partial messages for each FD
fd_set  curr_sock, cpy_read, cpy_write; // The sets for select()
int     max_fd = 0; // The highest FD currently open
int     gid = 0; // Global ID counter for clients

// --- PROVIDED HELPER 1 (Do not change) ---
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

// --- PROVIDED HELPER 2 (Do not change) ---
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

// --- NEW HELPER: Error handling ---
void fatal_error() {
    write(2, "Fatal error\n", 12);
    exit(1);
}

// --- NEW HELPER: Send message to everyone except sender ---
void send_all(int sender_fd, char *str) {
    for (int fd = 0; fd <= max_fd; fd++) {
        // Check if FD is in our set AND it's not the sender
        if (FD_ISSET(fd, &curr_sock) && fd != sender_fd)
            send(fd, str, strlen(str), 0);
    }
}

int main(int argc, char **argv) {
	int sockfd, connfd;
	socklen_t len; // Changed int to socklen_t for correctness
	struct sockaddr_in servaddr, cli; 
    char buff[4096]; // Buffer for raw reads
    char msg_to_send[4200]; // Buffer for formatted messages

    // 1. ARGUMENT CHECK (Added)
    if (argc != 2) {
        write(2, "Wrong number of arguments\n", 26);
        exit(1);
    }

	// 2. SOCKET CREATION (From starter code, but with fatal_error)
	sockfd = socket(AF_INET, SOCK_STREAM, 0); 
	if (sockfd == -1) 
		fatal_error();
	bzero(&servaddr, sizeof(servaddr)); 

	// assign IP, PORT 
	servaddr.sin_family = AF_INET; 
	servaddr.sin_addr.s_addr = htonl(2130706433); // 127.0.0.1
	servaddr.sin_port = htons(atoi(argv[1])); // CHANGED: Use argv[1]
  
	// Binding newly created socket 
	if ((bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr))) != 0) 
		fatal_error();

	if (listen(sockfd, 10) != 0)
		fatal_error();

    // 3. SELECT INIT (Added)
    FD_ZERO(&curr_sock);
    FD_SET(sockfd, &curr_sock); // Add server socket to the set
    max_fd = sockfd;

    // 4. THE MAIN LOOP (Completely new structure)
    while (1) {
        // Refresh the sets because select is destructive
        cpy_read = curr_sock;
        cpy_write = curr_sock;

        // Wait for activity
        if (select(max_fd + 1, &cpy_read, &cpy_write, 0, 0) < 0)
            continue;

        // Check every file descriptor to see if it's ready
        for (int fd = 0; fd <= max_fd; fd++) {
            
            // If this FD has data to read...
            if (FD_ISSET(fd, &cpy_read)) {
                
                // CASE A: It's the server socket -> New Connection
                if (fd == sockfd) {
                    len = sizeof(cli);
                    connfd = accept(sockfd, (struct sockaddr *)&cli, &len);
                    if (connfd < 0) continue;

                    if (connfd > max_fd) max_fd = connfd;
                    
                    clients[connfd] = gid++; // Assign ID
                    msg_buffer[connfd] = NULL; // Init buffer
                    FD_SET(connfd, &curr_sock); // Add to set

                    sprintf(msg_to_send, "server: client %d just arrived\n", clients[connfd]);
                    send_all(connfd, msg_to_send);
                }
                // CASE B: It's a client -> Receive Data
                else {
                    int ret = recv(fd, buff, 4094, 0);
                    
                    // Client disconnected
                    if (ret <= 0) {
                        sprintf(msg_to_send, "server: client %d just left\n", clients[fd]);
                        send_all(fd, msg_to_send);
                        FD_CLR(fd, &curr_sock);
                        close(fd);
                        if (msg_buffer[fd]) free(msg_buffer[fd]); // Prevent leak
                    }
                    // Client sent data
                    else {
                        buff[ret] = 0; // Null terminate received data
                        msg_buffer[fd] = str_join(msg_buffer[fd], buff);
                        
                        char *msg = 0;
                        // Process all complete lines in buffer
                        while (extract_message(&msg_buffer[fd], &msg)) {
                            sprintf(msg_to_send, "client %d: %s", clients[fd], msg);
                            send_all(fd, msg_to_send);
                            free(msg); // Free the extracted line
                        }
                    }
                }
            }
        }
    }
    return (0);
}