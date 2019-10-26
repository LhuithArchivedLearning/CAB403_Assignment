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
#include <pthread.h>
#include <semaphore.h>

#include "printf_helper.h"

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

void sigint_handler(int sig)
{

	//printf("pid:%d AHHH SIGINT!\n", getpid());

	if (live_flag == 1)
	{
		live_flag = 0;
	}
	else
	{
		sig_flag = 0;
	}

	//exit(0);
}

//Write LIVEFEED --- READ LIVEFEED
//WRITE R/N ------ READ R/N

struct read_write_struct{
	
	char *w_buff;
	char *r_buff;

	int socket;

	sem_t *w_mute; //mutex for writing
	sem_t *r_mute; //mutex for reading
	sem_t *o_mute; //mutex for stdout
	
	size_t w_size;
	size_t r_size;

	int *n;
	volatile sig_atomic_t *sig_flag_ptr;
	volatile sig_atomic_t *live_flag_ptr;
	volatile sig_atomic_t *live_ptr;
};

pthread_mutex_t r_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t w_mutex = PTHREAD_MUTEX_INITIALIZER;

void* livefeed(void* struct_pass){
	
	struct read_write_struct *read_write = (struct read_write_struct*) struct_pass;

	//char read_buffer[MAX];
	//char write_buffer[MAX];

	while(*read_write->sig_flag_ptr){
			while (*read_write->live_ptr){
				
				strcpy(read_write->w_buff, "s");

				if (live_flag == 0){
					strcpy(read_write->w_buff, "d");
					*read_write->live_ptr = 0;
				}
				
				sem_post(read_write->w_mute);
					write(read_write->socket, read_write->w_buff, read_write->w_size);		
				sem_wait(read_write->w_mute);

				sem_post(read_write->r_mute);
					read(read_write->socket, read_write->r_buff, read_write->r_size);
				sem_wait(read_write->r_mute);

				
				if (strncmp(read_write->r_buff, "nc", 2) == 0){
					//*read_write->live_flag_ptr = 0;
					*read_write->live_flag_ptr = 0;
					*read_write->live_ptr = 0;
				} else if (strncmp(read_write->r_buff, "\0", 2) != 0){
					printf(YELLOW);
						printf("%s\n", read_write->r_buff);
					printf(RESET);
				}

			bzero(read_write->w_buff, MAX);
			bzero(read_write->r_buff, MAX);	
			sleep(1);
		}
	}

	printf("exiting.\n");
	pthread_exit(0);
}

void client_chat(int sockfd)
{
	pthread_t live_tid;
	sem_t w_mutex;
	sem_t r_mutex;
	sem_t o_mutex;

	client_socket = sockfd;
	
	char r_buff[MAX];
	char w_buff[MAX];

	int n = 0, f;

	sem_init(&w_mutex, 1, 1);

	//----------------- LIVE FEED ----------------------------
	struct read_write_struct *read_write = malloc(sizeof(struct read_write_struct));

	read_write->r_buff = r_buff;
	read_write->r_size = sizeof(r_buff);
	read_write->r_mute = &r_mutex;

	read_write->w_buff = w_buff;
	read_write->w_size = sizeof(w_buff);
	read_write->w_mute = &w_mutex;

	read_write->socket = sockfd;
	read_write->o_mute = &o_mutex;
	read_write->n = &n;			
	read_write->sig_flag_ptr = &sig_flag;
	read_write->live_flag_ptr = &live_flag;
	read_write->live_ptr = &live;

	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_create(&live_tid, &attr, livefeed, read_write);

//----------------- LIVE FEED ----------------------------

	while (sig_flag){
		bzero(r_buff, MAX);
		bzero(w_buff, MAX);

		n = 0, f = 0;

		socket_info socketpass;
		socketpass.socket_fd = sockfd;
		
		//sem_post(&w_mutex);
			while (((w_buff[n++] = getchar()) != '\n') != 0 && sig_flag){}
		//sem_wait(&w_mutex);

		if ((strncmp(w_buff, "BYE", 3)) == 0){
			break;
		} else if ((strncmp(w_buff, "LIVEFEED", 8)) == 0 && !live_flag){
			//say LIVEFEED
		
			sem_post(&w_mutex);
				write(sockfd, w_buff, sizeof(w_buff));
			sem_wait(&w_mutex);

			//bzero(w_buff, MAX);

			//read wetther to go or not

			sem_post(&r_mutex);
				read(sockfd, r_buff, sizeof(r_buff));
			sem_wait(&r_mutex);

			if (strncmp(r_buff, "N", 1) == 0){
				printf("Not subscribed to any channels.\n");
				live_flag = 0;
				live = 0;
			}
			else {
				printf("Going live\n");
				live_flag = 1;	
				live = 1;
			}
			
			bzero(r_buff, MAX);
			bzero(w_buff, MAX);
			continue;

		} else if ((strncmp(w_buff, "CHANNELS", 8)) == 0){
			int channel_live = 1;
			//say LIVEFEED

			sem_post(&w_mutex);
				write(sockfd, w_buff, sizeof(w_buff));
			sem_wait(&w_mutex);
			
			//bzero(w_buff, MAX);

			//read wetther to go or not
			
			sem_post(&r_mutex);
				read(sockfd, r_buff, sizeof(r_buff));
			sem_wait(&r_mutex);
			

			if (strncmp(r_buff, "N", 1) == 0){
				printf("Not subscribed to any channels.\n");
				//live_flag = 0;
				channel_live = 0;
			}

			else {
				//live_flag = 1;
			}

			while (channel_live){

				strcpy(w_buff, "s");

				if (live_flag == 0){
					//strcpy(buff, "d");
					//live = 0;
				}

				sem_post(&w_mutex);
					write(sockfd, w_buff, sizeof(w_buff));
				sem_wait(&w_mutex);

				//bzero(w_buff, MAX);

				
				sem_post(&r_mutex);
					read(sockfd, r_buff, sizeof(r_buff));
				sem_wait(&r_mutex);
				

				if (strncmp(r_buff, "d", 1) == 0){
					channel_live = 0;
				} else {
					printf("%s\n", r_buff);
				}
					
				bzero(r_buff, MAX);
				bzero(w_buff, MAX);
			}
			
			bzero(r_buff, MAX);
			bzero(w_buff, MAX);
			continue;
		} else if ((strncmp(w_buff, "STOP", 4)) == 0){
			if(live_flag){
				printf("Stopping Livefeed.\n");
				live_flag = 0;
			}
		}

		sem_post(&w_mutex);
			write(sockfd, w_buff, sizeof(w_buff));
		sem_wait(&w_mutex);

		//bzero(buff, MAX);

		sem_post(&r_mutex);
			if (f = read(sockfd, r_buff, sizeof(r_buff)) == -1){
				printf("%s", "Lost Connection.\n");
				break;
			}
		sem_wait(&r_mutex);

		//From Server
		if (strncmp(r_buff, "\0", 2) != 0){
			printf(BLUE);
				printf("%s", r_buff);
			printf(RESET);
		}

		bzero(r_buff, MAX);
		bzero(w_buff, MAX);
	}

	client_exit();
	strcpy(w_buff, "BYE\n");

	sem_post(&w_mutex);
		write(sockfd, w_buff, sizeof(w_buff));
	sem_wait(&w_mutex);
	
	bzero(w_buff, MAX);
	//sem_destroy(&mutex);
	free(read_write);
	printf("Threads %u closing.\n", live_tid);
	pthread_join(live_tid, NULL);

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
		herror("gethostbyname");
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
