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
 	printf("Client Exit...\n"); 
}


typedef struct socket_info_struct{
	int socket_fd;
} socket_info;

volatile sig_atomic_t sig_flag = 1;
volatile sig_atomic_t live_flag = 0;

void sigint_handler(int sig){
	

	//printf("pid:%d AHHH SIGINT!\n", getpid());

	if(live_flag == 1){ 

		live_flag = 0; 
	} else {

		sig_flag = 0;
	}

	//exit(0);
}

void client_chat(int sockfd) 
{ 
	client_socket = sockfd;
    char buff[MAX]; 
    int n, f; 

    while (sig_flag) { 
		
		bzero(buff, MAX); 
        n = 0, f = 0; 

		socket_info socketpass;
		socketpass.socket_fd = sockfd;

		while (((buff[n++] = getchar()) != '\n') != 0 && sig_flag);

		if ((strncmp(buff, "BYE", 3)) == 0) { 
			break; 
        } else if ((strncmp(buff, "LIVEFEED", 8)) == 0) { 
			
			live_flag = 1;

			write(sockfd, buff, sizeof(buff));
			bzero(buff, MAX);
			
			while(1){
				
				if(live_flag == 0){strcpy(buff, "SIG_MES\0"); break;}

				if(f = read(sockfd, buff, sizeof(buff)) < 0){ printf("Connection Lost.\n"); break;}
				
				if(strncmp(buff, "/1", 1) == 0){ 
					printf("%s\n", "Not Subbed to channel");
					live_flag = 0;
					bzero(buff, MAX);
					break;
				}

				if(strncmp(buff, "\0", 2) != 0){ 
					printf("%s\n", buff);
				}
				strcpy(buff, "\0");
				write(sockfd, buff, sizeof(buff)); 
				bzero(buff, MAX);
			}
			n = 0;
		}
		
        write(sockfd, buff, sizeof(buff)); 
		bzero(buff, MAX);

		if(f = read(sockfd, buff, sizeof(buff)) == -1){
			printf("%s", "Lost Connection."); 
			break;
		}

		//From Server
        if(strncmp(buff, "\0", 2) != 0) {printf("%s", buff);};
		bzero(buff, MAX); 
    } 

	client_exit();
	strcpy(buff, "BYE\n");
	write(sockfd, buff, sizeof(buff)); 
	bzero(buff, MAX);
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
	printf("%s", buf);


	void sigint_handler(int sig); /*prototype*/
	struct sigaction sa;

	sa.sa_handler = sigint_handler;
	sa.sa_flags = 0; // or SA_RESTART
	sigemptyset(&sa.sa_mask);

	if(sigaction(SIGINT, &sa, NULL) == -1){
		perror("sigaction");
		exit(1);
	}

	client_chat(sockfd);

	close(sockfd);

	return 0;
}
