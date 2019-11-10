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

#define h_addr h_addr_list[0]
#define MAXDATASIZE 1250 /* max number of bytes we can get at once */

#define MAX 1250

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


void sigint_handler(int sig){

	if (live_flag){
		live_flag = 0;

		char m[3] = "SIG";
		write(client_socket, m, sizeof(m));
	} else {
		sig_flag = 0;
	}
}


struct read_write_struct{
	int socket;
};

//Read thread that listens in the background, 
//SIG comes from server connection close
//Exiting comes from live thread closing from server
//else it will print responses from the server
void* read_thread(void* struct_pass){
	
	struct read_write_struct *read_write = (struct read_write_struct*) struct_pass;

	char r_buff[MAX];

	bzero(r_buff, MAX);
	while(sig_flag){
			bzero(r_buff, MAX);

			if((read(read_write->socket, r_buff, sizeof(r_buff))) == -1){
				strcpy(r_buff, "SIG");
			}

			if(strncmp(r_buff, "exiting", 7) == 0){
				live_flag = 0;
				printf(GREEN);
					fprintf(stdout, "%s", r_buff);
				printf(RESET);
				fflush(stdout);

			} else if(strncmp(r_buff, "SIG", 3) == 0){
				sig_flag = 0;
				live_flag = 0;

				printf(RED);
					printf("%s\n", "Lost Connection To Server.");
					fflush(stdin);
					fflush(stdout);
					fprintf(stdin, "%c", "\n\n\n");
				printf(RESET);
				break;
			} else if(strncmp(r_buff, "\0", 2) != 0){
				printf(CYAN);
					fprintf(stdout, "%s", r_buff);
					
				printf(RESET);
				fflush(stdout);
			} 

			bzero(r_buff, MAX);
	}

	printf("Closing Read Thread.\n");
	pthread_exit(0);
}


void client_chat(int sockfd){

	pthread_t read_tid, resolver_tid;

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
	int args = 0;

	while (sig_flag){

			bzero(w_buff, MAX);
			n = 0, f = 0, args = 0;

			while (((w_buff[n++] = getchar()) != '\n') != 0 && sig_flag && n <= MAX - 1){}

			strcpy(parse_string, w_buff);

			args = parse_input(parse_string, " ", argv);

			//not needed but nice, incase message is large then post size
			if(args > 2){ if(strlen(argv[2]) > 1024){printf("MAX exceeded.\n");}}

			//get rid of any symbols from sig catch 
			string_remove_nonalpha(argv[0]);

			//close client loop and begin closing process
			if ((strncmp(argv[0], "BYE", 3)) == 0){
				sig_flag = 0;
				live_flag = 0;
			} else if ((strncmp(argv[0], "LIVEFEED", 8)) == 0 && !live_flag) {
				live_flag = 1;
			} 
			
			write(sockfd, w_buff, sizeof(w_buff));
			bzero(w_buff, MAX);
	}

	printf("Threads %lu closing.\n", read_tid);
	pthread_join(read_tid, NULL);

	free(read_write);
	
	//final bye incase server is still listening
	strcpy(w_buff, "BYE\n");
	write(sockfd, w_buff, sizeof(w_buff));
	bzero(w_buff, MAX);

	client_exit();
}

int main(int argc, char *argv[]){
	int sockfd, numbytes, port;
	char buf[MAXDATASIZE];
	struct hostent *he;
	struct sockaddr_in their_addr; /* connector's address information */

	if (argc != 3){
		fprintf(stderr, "usage: client hostname & port #\n");
		exit(1);
	}

	if ((he = gethostbyname(argv[1])) == NULL){ /* get the host info */
		perror("gethostbyname");
		exit(1);
	}

	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1){
		perror("socket");
		exit(1);
	}
	//port = atoi(argv[2]);
	if (atoi(argv[2]) == 0){
		perror("port");
		exit(1);
	}

	port = atoi(argv[2]);

	their_addr.sin_family = AF_INET;   /* host byte order */
	their_addr.sin_port = htons(port); /* short, network byte order */
	their_addr.sin_addr = *((struct in_addr *)he->h_addr);
	bzero(&(their_addr.sin_zero), 8); /* zero the rest of the struct */

	if (connect(sockfd, (struct sockaddr *)&their_addr, sizeof(struct sockaddr)) == -1){
		perror("connect");
		exit(1);
	}

	if ((numbytes = recv(sockfd, buf, MAXDATASIZE, 0)) == -1){
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

	if (sigaction(SIGINT, &sa, NULL) == -1){
		perror("sigaction");
		exit(1);
	}

	client_chat(sockfd);

	close(sockfd);

	return 0;
}
