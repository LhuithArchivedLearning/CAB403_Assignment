/*
*  Materials downloaded from the web. See relevant web sites listed on OLT
*  Collected and modified for teaching purpose only by Jinglan Zhang, Aug. 2006
*/
#include <arpa/inet.h>
#include <stdio.h> 
#include <stdlib.h> 
#include <errno.h> 
#include <string.h> 
#include <sys/types.h> 
#include <netinet/in.h> 
#include <sys/socket.h> 
#include <sys/wait.h> 
#include <unistd.h> 
#include <ctype.h>
#include <sys/mman.h>
#include <signal.h>


#include "shared_mem_struct.h"
#include "shared.h"
#include "subscriber_struct.h"
#include "subscriber.h"
#include "helper.h"

//#define MYPORT 54321    /* the port users will be connecting to */
#define BACKLOG 10     /* how many pending connections queue will hold */
#define MAX 80 

int sockfd, new_fd;
callback display_channels = display;

volatile int pid_test = 0;

void SIGHANDLE(const int sig){

	if(sig == SIGINT){
		printf("pid %d\n", pid_test);
		
		if(pid_test == 0){clean_up_shared_mem();}

		exit(0);
	}
}

volatile sig_atomic_t sig_flag = 1;

void sigint_handler(int sig){

	sig_flag = 0;

	printf("pid:%d\n", getpid());
	if(pid_test == 0){clean_up_shared_mem();}

}

struct memory* memptr;

void no_subscription(char m[], char b[]){
	strcpy(m, "Not subscribed to any channels.");
	strcat(m, "\n");
	strcpy(b, m);
}

void wrong_args(char b[]){
	char m[32] = {"Wrong Number of Arguments.\n"};
	strcpy(b, m);
}

void write_to_buff(char b[], char *s){
	char m[32];
	strcpy(m, s);
	strcat(m, "\n");
	strcpy(b, m);
}

void subscribe(client* c, int id, char buff[]){
		subbed_channel* sub_tmp = NULL;

		//Passing Subbing information 
		//-------------------------------------
		char message[32] = {"Subscribed to channel "};

		sub_tmp = search(c->head, id);

		if(sub_tmp != NULL){
			strcpy(message, "Already Subscribed to ");
			cancat_int(message, c->head->channel_id);
			strcat(message, "\n");
			strcpy(buff, message);
			return;
		}

		//-------------------------------------
		c->head = prepend(c->head, id);
		
		channel *cur_channel = &memptr->channels[id];
		c->head->read_index = cur_channel->post_index;

		cancat_int(message, c->head->channel_id);

		strcat(message, "\n");
		strcpy(buff, message);
}

void unsubscribe(client* c, int id, char buff[]){

	subbed_channel* sub_tmp = NULL;

	char message[32] = {"Unsubscribing from "};

	if(c->head == NULL){
		no_subscription(message, buff);
		return;
	} 

	sub_tmp = search(c->head, id);

	if(sub_tmp == NULL){
		strcpy(message, "Not Subbed to channel ");
	} else {
		c->head = remove_sub(c->head, sub_tmp);
		traverse(c->head, display);
	}

	cancat_int(message, id);

	strcat(message, "\n");
	strcpy(buff, message);			
}

void next(client* c, int id, char buff[]){
	
	subbed_channel* sub_tmp = NULL;

	char message[32] = {""};

	if(c->head == NULL){
		no_subscription(message, buff);
		return;
	} 
	
	sub_tmp = search(c->head, id);

	if(sub_tmp == NULL){
		strcpy(message, "Not subscribed to channel ");
		cancat_int(message, id);
	} else {
		channel *cur_channel = &memptr->channels[id];
		
		cancat_int(message, id);
		strcat(message, ":");
	
		if(sub_tmp->read_index < cur_channel->post_index){
			strcat(message, cur_channel->posts[sub_tmp->read_index++].message);
		} else {			
			//strcat(message, "No New Messages");
		}	
	}

	strcat(message, "\n");
	strcpy(buff, message);
}

void Channels(client* c, int id, int socket, char reader[]){

	subbed_channel* sub_tmp = NULL;
	sub_tmp = search(c->head, id);

	if(sub_tmp == NULL){
		printf("no channels");
	} else {
			while(sub_tmp != NULL){
			printf("inlive: %d\n", sub_tmp->channel_id);
			sub_tmp = sub_tmp->next;
		}
	}
}

void livefeed(client* c, int id, int s, char r[], char a[], size_t s_r, size_t s_a){
	subbed_channel* cursor = NULL;
	cursor = c->head;

	while(1){

		if(!sig_flag){break;}
		
		read(s, r, s_r); 

		if (strncmp(r, "SIG_MES", 7) == 0){ 
			strcpy(a, "\0");
			printf("Exit Command.\n"); 
			break;
		}

		channel *cur_channel = &memptr->channels[cursor->channel_id];
		
		if(cursor->read_index < cur_channel->post_index){
				char m[32] = "";
				cancat_int(m, cursor->channel_id);
				strcat(m, ":");
				strcat(m, cur_channel->posts[cursor->read_index++].message);
				strcpy(a, m);
		} else {
			strcpy(a, "\0");
		}

		write(s, a, s_a);
		
		bzero(a, MAX);
		bzero(r, MAX);
		
		if(cursor->next != NULL){cursor = cursor->next;} else {cursor = c->head;}
	}
}

void server_chat(int sockfd, int c) { 
    char read_buff[MAX]; 
	char answer_buff[MAX];
	char* argv[5];
	int argc, channel_id = 0;

	client* new_client = malloc(sizeof(client));
	new_client->client_id = c;
	new_client->head = NULL;
	subbed_channel* tmp = NULL;
	printf("Client ID is: %d\n", new_client->client_id);

    // infinite loop for chat 
    while (sig_flag) { 

        bzero(read_buff, MAX); 
		bzero(answer_buff, MAX);
		
		strcpy(answer_buff, "Input Not Found.\n");

        // read the message from client and copy it in buffer 
        if (read(sockfd, read_buff, sizeof(read_buff)) < 0){
			continue;
		} 

		if (strncmp(read_buff, "\0", 2) == 0) { 
			strcpy(answer_buff, "");
		}

		printf("client sent: %s", read_buff);

		//printf("from client: %s", read_buff);
		// print buffer which contains the client contents 
		argc = parse_input(read_buff, " ", argv);
		
		if(argc >= 2){
			channel_id = is_numeric(argv[1]);
		}

		// if msg contains "Exit" then server exit and chat ended. 
        if (strncmp(argv[0], "BYE", 3) == 0) { 
           
		    printf("Closing Client <%d>...\n", c); 		
			
			clean_up(argv, argc);

			sub_dispose(new_client->head);
			free(new_client);
            
			break; 
        } else if (strncmp(argv[0], "SUB", 3) == 0) { 
			
			if(argc < 2){
				wrong_args(answer_buff); 
			} else {
				if(channel_id == -1){
					write_to_buff(answer_buff, "Please Use Numerical Values.");
				} else {
					subscribe(new_client, channel_id, answer_buff);
				}				
			}
		} else if (strncmp(argv[0], "UNSUB", 5) == 0) { 
			unsubscribe(new_client, channel_id, answer_buff);
		} else if (strncmp(argv[0], "NEXT", 4) == 0) { 
			next(new_client, channel_id, answer_buff);
		} else if (strncmp(argv[0],"LIVEFEED", 8) == 0) {

			//Reading LIVEFEED

			if(new_client->head == NULL){
				//Cant LiveFeed, Answers with N
				strcpy(answer_buff, "N");
			} else {
				int live = 1;
				subbed_channel* cursor = NULL;
				cursor = new_client->head;	
				char m[32] = "";

				//confirming with the client to start livefeed
				strcpy(answer_buff, "R");
				write(sockfd, answer_buff, sizeof(answer_buff));
				bzero(answer_buff, MAX);
				bzero(read_buff, MAX);

				while(live){
					//now read, then write
					read(sockfd, read_buff, sizeof(read_buff));
					
					//Handling SIG
					if(strncmp(read_buff, "SIG", 3) == 0){

					}

					//stream read, if stream is broken please stapt after
					if(strncmp(read_buff, "s", 1) == 0){
					} else {
						live = 0;
					}

					channel *cur_channel = &memptr->channels[cursor->channel_id];

					if(cursor->read_index < cur_channel->post_index){
						memset(m, 0, sizeof(m)); //clear message buffer
						cancat_int(m, cursor->channel_id);
						strcat(m, ":");
						strcat(m, cur_channel->posts[cursor->read_index++].message);
						strcpy(answer_buff, m);
					} else {
						strcpy(answer_buff, "\0");
					}

					write(sockfd, answer_buff, sizeof(answer_buff));
					
					if(cursor->next != NULL){cursor = cursor->next;} else {cursor = new_client->head;}

					bzero(answer_buff, MAX);
					bzero(read_buff, MAX);
				}

				continue;
			}


		} else if (strncmp(argv[0], "SEND", 4) == 0) { 
			
			if(argc != 3){
				wrong_args(answer_buff);
			} else {
							
				channel *cur_channel = &memptr->channels[channel_id];

				remove_substring(argv[2], "\n");
				strcpy(cur_channel->posts[cur_channel->post_index++].message, argv[2]);

				char message[25] = {"Client #"};
				cancat_int(message, c);
				strcat(message, ": ");

				strcat(message, read_buff);


				strcpy(answer_buff, "\0");
				//cancat_int(answer_buff,  channel_id);
				//strcat(answer_buff, "\n");
			}

		} else if (strncmp(argv[0], "CHANNELS", 8) == 0) { 
			//Reading LIVEFEED

			if(new_client->head == NULL){
				//Cant LiveFeed, Answers with N
				strcpy(answer_buff, "N");
			} else {
				int live = 1;
				subbed_channel* cursor = NULL;
				cursor = new_client->head;	
				char m[32] = "";

				//confirming with the client to start livefeed
				strcpy(answer_buff, "R");
				write(sockfd, answer_buff, sizeof(answer_buff));
				bzero(answer_buff, MAX);
				bzero(read_buff, MAX);

				while(live){
					//now read, then write
					read(sockfd, read_buff, sizeof(read_buff));
					
					//Handling SIG
					if(strncmp(read_buff, "SIG", 3) == 0){

					}

					//stream read, if stream is broken please stapt after
					if(strncmp(read_buff, "s", 1) == 0){
					} else {
						live = 0;
					}

					if(cursor != NULL){
						
						channel *cur_channel = &memptr->channels[cursor->channel_id];

						memset(m, 0, sizeof(m)); //clear message buffer
						cancat_int(m, cursor->channel_id);
						strcat(m, ":");
						strcat(m, "\t");
						cancat_int(m, cur_channel->post_index);
						strcat(m, "\t");
						cancat_int(m, cursor->read_index);
						strcat(m, "\t");
						cancat_int(m, cur_channel->post_index - cursor->read_index);
						strcpy(answer_buff, m);
						cursor = cursor->next;
						
					} else { 
						live = 0;
						strcpy(answer_buff, "d");
					}
					
					write(sockfd, answer_buff, sizeof(answer_buff));

					bzero(answer_buff, MAX);
					bzero(read_buff, MAX);
				}
				continue;
			}
		} 

		write(sockfd, answer_buff, sizeof(answer_buff));
		bzero(answer_buff, MAX);
		bzero(read_buff, MAX);
    } 
} 


int main(int argc, char *argv[]){
	
	int port;  /* listen on sock_fd, new connection on new_fd */
	struct sockaddr_in my_addr;    /* my address information */
	struct sockaddr_in their_addr; /* connector's address information */
	socklen_t sin_size;

	//latching to that sweet pointer
	memptr = create_shared_mem("/temp");

	/* generate the socket */
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		perror("socket");
		exit(1);
	}

	if(argc != 2){
		fprintf(stderr,"usage: port #\n");
		exit(1);
	}

	//port = atoi(argv[2]);
	if(atoi(argv[1]) == 0){perror("port"); exit(1);}

	port = atoi(argv[1]);
	/* generate the end point */
	my_addr.sin_family = AF_INET;         /* host byte order */
	my_addr.sin_port = htons(port);     /* short, network byte order */
	my_addr.sin_addr.s_addr = INADDR_ANY; /* auto-fill with my IP */
	/* bzero(&(my_addr.sin_zero), 8);   ZJL*/     /* zero the rest of the struct */

	/* bind the socket to the end point */
	if (bind(sockfd, (struct sockaddr *)&my_addr, sizeof(struct sockaddr)) \
	== -1) {
		perror("bind");
		exit(1);
	}

	/* start listnening */
	if (listen(sockfd, BACKLOG) == -1) { perror("listen"); exit(1); }

	printf("Server Started...\n"); 

	/* repeat: accept, send, close the connection */
	/* for every accepted connection, use a sepetate process or thread to serve it */

	
					
	void sigint_handler(int sig); /*prototype*/
	struct sigaction sa;

	sa.sa_handler = sigint_handler;
	sa.sa_flags = 0; // or SA_RESTART
	sigemptyset(&sa.sa_mask);

	if(sigaction(SIGINT, &sa, NULL) == -1){
		perror("sigaction");
		exit(1);
	}

	while(sig_flag) {  /* main accept() loop */

		sin_size = sizeof(struct sockaddr_in);

		if ((new_fd = accept(sockfd, (struct sockaddr *)&their_addr, \
		&sin_size)) == -1) {
			//perror("poop");
			printf("Server Closeing...");
			continue;
		}

		printf("Server: got connection from %s\n", \
			inet_ntoa(their_addr.sin_addr));
		if (!fork()) { /* this is the child process */
			pid_test = getpid();

			/* WELCOME MESSAGE */	
			char message[255] = {"Welcome! Your Client ID is <"};
			int clientid = memptr->num_clients++;
			cancat_int(message, clientid);
			strcat(message, ">\n");
			size_t len = strlen(message);
			/* WELCOME MESSAGE */	
			
			if (send(new_fd, message, len, 0) == -1){ perror("send");}

			//chat with the client
			server_chat(new_fd, clientid);
			
			printf("server: closing connection from %s\n", inet_ntoa(their_addr.sin_addr));

			close(new_fd);

			exit(0);
		}

		close(new_fd);  /* parent doesn't need this */

		while(waitpid(-1, NULL,WNOHANG) > 0){}; /* clean up child processes */
	}

	return 0;		
}
