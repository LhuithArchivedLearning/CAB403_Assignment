#include <stdio.h> 
#include <stdlib.h> 
#include <errno.h> 
#include <string.h> 
#include <netdb.h> 
#include <sys/types.h> 
#include <netinet/in.h> 
#include <sys/socket.h> 
#include <unistd.h>
#include <signal.h>

//#define PORT 54321    /* the port client will be connecting to */
#define MAXDATASIZE 100 /* max number of bytes we can get at once */

#define MAX 80

int client_socket = 0;

void client_exit(){
 	printf("\nClient Exit...\n"); 
}


typedef struct socket_info_struct{
	int socket_fd;
} socket_info;


int SIGHANDLE(const int sig, void *ptr){

	if(sig == SIGINT){
		client_exit();

		char exit_message[5] = {"BYE\n"};
		
		write(client_socket, exit_message, sizeof(exit_message)); 
		exit(0);
	}

	return (0);
}

void client_chat(int sockfd) 
{ 
	client_socket = sockfd;
    char buff[MAX]; 
    int n; 
    for (;;) { 
        bzero(buff, sizeof(buff)); 
        printf("Enter the string : "); 
        n = 0; 

		socket_info socketpass;
		socketpass.socket_fd = sockfd;

		if(signal(SIGINT, (void(*)(int)) SIGHANDLE) == 0){}

		while ( ((buff[n++] = getchar()) != '\n'));

		if ((strncmp(buff, "BYE", 3)) == 0) { 
            client_exit();
			strcpy(buff, "Exit");
			write(sockfd, buff, sizeof(buff)); 
			bzero(buff, sizeof(buff)); 
			break; 
        }
		
        write(sockfd, buff, sizeof(buff)); 
        bzero(buff, sizeof(buff)); 
        read(sockfd, buff, sizeof(buff)); 
        printf("From Server : %s", buff); 
    } 
} 

int main(int argc, char *argv[])
{
	int sockfd, numbytes, port;  
	char buf[MAXDATASIZE];
	struct hostent *he;
	struct sockaddr_in their_addr; /* connector's address information */

	if (argc != 3) {
		fprintf(stderr,"usage: client hostname & port #\n");
		exit(1);
	}

	if ((he=gethostbyname(argv[1])) == NULL) {  /* get the host info */
		herror("gethostbyname");
		exit(1);
	}

	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		perror("socket");
		exit(1);
	}
	//port = atoi(argv[2]);
	if(atoi(argv[2]) == 0){ perror("port"); exit(1);}
 	port = atoi(argv[2]);


	their_addr.sin_family = AF_INET;      /* host byte order */
	their_addr.sin_port = htons(port);    /* short, network byte order */
	their_addr.sin_addr = *((struct in_addr *)he->h_addr);
	bzero(&(their_addr.sin_zero), 8);     /* zero the rest of the struct */

	if (connect(sockfd, (struct sockaddr *)&their_addr, \
	sizeof(struct sockaddr)) == -1) {
		perror("connect");
		exit(1);
	}

	if ((numbytes=recv(sockfd, buf, MAXDATASIZE, 0)) == -1) {
		perror("recv");
		exit(1);
	}

	buf[numbytes] = '\0';
	printf("Received: %s", buf);

	client_chat(sockfd);


	close(sockfd);

	return 0;
}
