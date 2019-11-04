#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <strings.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>

#include <signal.h>
#include "printf_helper.h"
#include "helper.h"
#include "worker.h"

#define h_addr h_addr_list[0]
//#define PORT 54321    /* the port client will be connecting to */
#define MAXDATASIZE 100 /* max number of bytes we can get at once */

#define MAX 80

int client_socket = 0;

void client_exit()
{
	printf("Client Exit...\n");
}

typedef struct socket_info_struct
{
	int socket_fd;
} socket_info;

volatile sig_atomic_t sig_flag = 1;
volatile sig_atomic_t live_flag = 0;
volatile sig_atomic_t live = 0;
volatile sig_atomic_t next_flag = 0;

void sigint_handler(int sig){
	if (live_flag == 1){
		live_flag = 0;
	}
	else{
		sig_flag = 0;
	}
}


struct read_write_struct{
	int socket;
};

void* read_thread(void* struct_pass){
	
	struct read_write_struct *read_write = (struct read_write_struct*) struct_pass;

	char r_buff[MAX];

	bzero(r_buff, MAX);

	while(sig_flag){

			read(read_write->socket, r_buff, sizeof(r_buff));

			if(strncmp(r_buff, "\0", 2) != 0){
				printf(CYAN);
					fprintf(stdout, "%s\n", r_buff);
				printf(RESET);
			}

			bzero(r_buff, MAX);
	}

	bzero(r_buff, MAX);
	
	pthread_exit(0);
}



void client_chat(int sockfd){

	pthread_t read_tid;

	client_socket = sockfd;

	char w_buff[MAX];

	int n = 0, f;

	//----------------- READ THREAD ----------------------------
	struct read_write_struct *read_write = malloc(sizeof(struct read_write_struct));

	read_write->socket = sockfd;

	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_create(&read_tid, &attr, read_thread, read_write);
	
	//----------------- READ THREAD ----------------------------

	char* argv[5];
	char parse_string[MAX];
	
	while (sig_flag){
		//bzero(r_buff, MAX);
		bzero(w_buff, MAX);

		n = 0, f = 0;

		socket_info socketpass;
		socketpass.socket_fd = sockfd;
		
		while (((w_buff[n++] = getchar()) != '\n') != 0 && sig_flag){}

		strcpy(parse_string, w_buff);

		parse_input(parse_string, " ", argv);

		if ((strncmp(w_buff, "BYE", 3)) == 0){
			break;
		} else if ((strncmp(argv[0], "LIVEFEED", 8)) == 0){
			if(!live_flag){
				live_flag = 1;
			}
		} else if ((strncmp(w_buff, "CHANNELS", 8)) == 0){

		} else if ((strncmp(w_buff, "STOP", 4)) == 0){
			if(live_flag){
				live_flag = 0;
			}
				
		} else if ((strncmp(w_buff, "NEXT", 4)) == 0){

		}

		write(sockfd, w_buff, sizeof(w_buff));

		bzero(w_buff, MAX);
	}
	
	bzero(w_buff, MAX);


	client_exit();
	strcpy(w_buff, "BYE\n");
	
	write(sockfd, w_buff, sizeof(w_buff));
	
	bzero(w_buff, MAX);
	
	printf("Threads %lu closing.\n", read_tid);
	free(read_write);
	pthread_join(read_tid, NULL);
}

int main(int argc, char *argv[]){
	int sockfd, numbytes, port;
	char buf[MAXDATASIZE];
	struct hostent *he;
	struct sockaddr_in their_addr; /* connector's address information */

	if (argc != 3)
	{
		fprintf(stderr, "usage: client hostname & port #\n");
		exit(1);
	}

	if ((he = gethostbyname(argv[1])) == NULL)
	{ /* get the host info */
		perror("gethostbyname");
		exit(1);
	}

	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
	{
		perror("socket");
		exit(1);
	}
	//port = atoi(argv[2]);
	if (atoi(argv[2]) == 0)
	{
		perror("port");
		exit(1);
	}
	port = atoi(argv[2]);

	their_addr.sin_family = AF_INET;   /* host byte order */
	their_addr.sin_port = htons(port); /* short, network byte order */
	their_addr.sin_addr = *((struct in_addr *)he->h_addr);
	bzero(&(their_addr.sin_zero), 8); /* zero the rest of the struct */

	if (connect(sockfd, (struct sockaddr *)&their_addr,
				sizeof(struct sockaddr)) == -1)
	{
		perror("connect");
		exit(1);
	}

	if ((numbytes = recv(sockfd, buf, MAXDATASIZE, 0)) == -1)
	{
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

	if (sigaction(SIGINT, &sa, NULL) == -1)
	{
		perror("sigaction");
		exit(1);
	}

	client_chat(sockfd);

	close(sockfd);

	return 0;
}
