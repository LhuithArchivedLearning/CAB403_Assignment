/*
*  Materials downloaded from the web. See relevant web sites listed on OLT
*  Collected and modified for teaching purpose only by Jinglan Zhang, Aug. 2006
*/
#include <arpa/inet.h>
#include <stdio.h> 
#include <stdlib.h> 
#include <errno.h> 
#include <strings.h> 
#include <sys/types.h> 
#include <netinet/in.h> 
#include <sys/socket.h> 
#include <sys/wait.h> 
#include <unistd.h> 
#include <ctype.h>
#include <sys/mman.h>

#include <pthread.h>
#include <semaphore.h>
#include <signal.h>

#include "printf_helper.h"
#include "shared_mem_struct.h"
#include "shared.h"
#include "subscriber_struct.h"
#include "subscriber.h"
#include "helper.h"
#define h_addr h_addr_list[0]
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
volatile sig_atomic_t live_flag = 0;
volatile sig_atomic_t live = 0;

volatile sig_atomic_t next_flag = 0;

void sigint_handler(int sig){
		live_flag = 0;
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
	//printf("id : %d\n", id);
	
	subbed_channel* sub_tmp = NULL;

	char message[32] = {""};

	if(c->head == NULL){
		no_subscription(message, buff);
		return;
	} 
	

	//if they give me a number
	if(id > 0){
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
	} else {
		//strcpy(message, "Getting Next Message I GUESS!.");
		sub_tmp = c->head;//search(c->head, id);

		while(sub_tmp != NULL){

			channel *cur_channel = &memptr->channels[sub_tmp->channel_id];	

			if(sub_tmp->read_index < cur_channel->post_index){
				cancat_int(message, sub_tmp->channel_id);
				strcat(message, ":");
				strcat(message, cur_channel->posts[sub_tmp->read_index++].message);
				break;
			} else {			
				sub_tmp = sub_tmp->next;
			}
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


struct read_write_struct{
	client *c;
	struct memory* memptr;
	int *id;
	pthread_t *tid;

	char *w_buff;
	char *r_buff;

	size_t w_size;
	size_t r_size;

	sem_t *w_mute; //mutex for writing
	sem_t *r_mute; //mutex for reading
	sem_t *o_mute; //mutex for stdout
	
	int socket;

	volatile sig_atomic_t *sig_flag_ptr;
	volatile sig_atomic_t *live_flag_ptr;
	volatile sig_atomic_t *live_ptr;

	volatile sig_atomic_t *next_flag_ptr;
};

//pthread_mutex_t r_mutex = PTHREAD_MUTEX_INITIALIZER;
//pthread_mutex_t w_mutex = PTHREAD_MUTEX_INITIALIZER;

sem_t mutex;

void locked_cpy(char b[], char m[]){
	//pthread_mutex_lock(&w_mutex);
		strcpy(b, m);
	//pthread_mutex_unlock(&w_mutex);
}


void* livefeed_thread(void* struct_pass){
	
	struct read_write_struct *read_write = (struct read_write_struct*) struct_pass;

	char r_buff[MAX];
	char w_buff[MAX];
	char m[32] = "";
	
	bzero(r_buff, MAX);
	bzero(w_buff, MAX);

	subbed_channel* cursor;

	while(sig_flag){
		while (*read_write->live_ptr){

			if(cursor == NULL && read_write->c->head != NULL){
				cursor = read_write->c->head;
			} 
			
				sleep(1);
				//read
				

				read(read_write->socket, read_write->r_buff, read_write->r_size);
			
					printf("%s\n", read_write->r_buff);

				//write
				strcpy(w_buff, "w");
				
				//meat and potatoes
				if(*read_write->id < 0){
					strcpy(w_buff, "reading from all.");
	
					//---------------------------------------- READING ALL ---------------------------
					if(cursor != NULL && read_write->c->head != NULL){

							channel *cur_channel = &read_write->memptr->channels[cursor->channel_id];
						
							if(cursor->read_index < cur_channel->post_index){
									memset(m, 0, sizeof(m)); //clear message buffer
									cancat_int(m, cursor->channel_id);
									strcat(m, ":");
									strcat(m, cur_channel->posts[cursor->read_index++].message);
									strcpy(w_buff, m);
							} else {
								strcpy(w_buff, "w");
							}

							if(cursor->next != NULL){cursor = cursor->next;} else {cursor = read_write->c->head;}
						
						} else {
							////printf("no channels :| %d.\n", *read_write->live_flag_ptr);
							//strcpy(write_buffer, "nc");
							//cursor = NULL;
							//*read_write->live_ptr = 0;
							//*read_write->live_flag_ptr = 0;
						}
					//---------------------------------------- READING ALL ---------------------------

				} else {
					channel *cur_channel = &memptr->channels[*read_write->id];
					
					cursor = search(read_write->c->head, *read_write->id);

					if(cursor->read_index < cur_channel->post_index){
						memset(m, 0, sizeof(m)); //clear message buffer
						cancat_int(m, *read_write->id);
						strcat(m, ":");
						strcat(m, cur_channel->posts[cursor->read_index++].message);
						strcpy(w_buff, m);
					} else {			
						strcpy(w_buff, "w");
					}	
				}

				if(strncmp(r_buff, "d", 1) == 0){
					*read_write->live_ptr = 0;
					*read_write->live_flag_ptr = 0;
					strcpy(w_buff, "z");
				}

				write(read_write->socket, w_buff, sizeof(w_buff));

				bzero(r_buff, MAX);
				bzero(w_buff, MAX);
			}
		}
}

void* livefeed(void* struct_pass){

	struct read_write_struct *read_write = (struct read_write_struct*) struct_pass;

	char read_buffer[MAX];
	char write_buffer[MAX];

	subbed_channel* cursor = read_write->c->head;

	char m[32] = "";


	//keep running
	//and when the flags align, run the livefeed
	while(*read_write->sig_flag_ptr){
			while(*read_write->live_ptr){


			if(cursor == NULL && read_write->c->head != NULL){
				//printf("Assigning i guess?\n");
				cursor = read_write->c->head;
			} 
			
			//now read, then write
			sem_wait(read_write->r_mute);
				read(read_write->socket, read_buffer, sizeof(read_buffer));
			sem_post(read_write->r_mute);

			//stream read, if stream is broken please stapt after
			if(strncmp(read_buffer, "s", 1) == 0){
				printf(YELLOW);
					//printf("thread %lu getting\n", *read_write->tid);
				printf(RESET);
			}
			
			if (strncmp(read_buffer, "d", 1) == 0){
				printf("thread %lu exiting s\n", *read_write->tid);
				//cursor = NULL;
				*read_write->live_ptr = 0;
				*read_write->live_flag_ptr = 0;
			}

			//Handling SIG
			if(strncmp(read_buffer, "SIG", 3) == 0){
		
			}
			
			if(cursor != NULL && read_write->c->head != NULL){
				//printf("working i guess?\n");
				
				channel *cur_channel = &read_write->memptr->channels[cursor->channel_id];
			
				if(cursor->read_index < cur_channel->post_index){
						memset(m, 0, sizeof(m)); //clear message buffer
						cancat_int(m, cursor->channel_id);
						strcat(m, ":");
						strcat(m, cur_channel->posts[cursor->read_index++].message);
						strcpy(write_buffer, m);
				
				//printf("working after i guess?\n");

				} else {
					strcpy(write_buffer, "\0");
				}

				if(cursor->next != NULL){cursor = cursor->next;} else {cursor = read_write->c->head;}
			
			} else {
				//printf("no channels :| %d.\n", *read_write->live_flag_ptr);
				strcpy(write_buffer, "nc");
				cursor = NULL;
				*read_write->live_ptr = 0;
				*read_write->live_flag_ptr = 0;
			}
			
			sem_wait(read_write->w_mute);
				write(read_write->socket, write_buffer, sizeof(write_buffer));
			sem_post(read_write->w_mute);

	
			bzero(read_buffer, MAX);
			bzero(write_buffer, MAX);
			sleep(1);
		}
	}

	printf("exiting.\n");
	pthread_exit(0);
}

void* next_thread(void* struct_pass){
	
	//printf("id : %d\n", id);
	struct read_write_struct *read_write = (struct read_write_struct*) struct_pass;

	subbed_channel* sub_tmp = NULL;

	char message[32] = {""};
	char w_buff[MAX];

	while(sig_flag){
		if(*read_write->next_flag_ptr){

			if(sub_tmp == NULL && read_write->c->head != NULL){
				sub_tmp = read_write->c->head;
			} 

			if(read_write->c->head != NULL){
				//if they gave me a number
				if(*read_write->id != -1){
					
					sub_tmp = search(read_write->c->head, *read_write->id);

					if(sub_tmp == NULL){
						strcpy(message, "Not subscribed to channel ");
						cancat_int(message, *read_write->id);
						
					} else {
						channel *cur_channel = &memptr->channels[*read_write->id];
						
						cancat_int(message, *read_write->id);
						strcat(message, ":");
					
						if(sub_tmp->read_index < cur_channel->post_index){
							strcat(message, cur_channel->posts[sub_tmp->read_index++].message);
						} else {			
							strcat(message, "No New Messages");
						}

					}
					strcat(message, "\n");
				} else {
					printf("%s", "Getting Next Message I GUESS!.");
					//sub_tmp = read_write->c->head;//search(c->head, id);

					while(sub_tmp != NULL){

						channel *cur_channel = &memptr->channels[sub_tmp->channel_id];	

						if(sub_tmp->read_index < cur_channel->post_index){
							cancat_int(message, sub_tmp->channel_id);
							strcat(message, ":");
							strcat(message, cur_channel->posts[sub_tmp->read_index++].message);
							break;
						} else {			
							sub_tmp = sub_tmp->next;
						}
					}
				}
			} else {
				strcpy(message , "Not subscribed to channels.");	
			}

			strcpy(read_write->w_buff, message);

			write(read_write->socket, w_buff, sizeof(w_buff));

			*read_write->next_flag_ptr = 0;
		}
	}

	pthread_exit(0);
}


void server_chat(int sockfd, int c) { 

    char read_buff[MAX]; 
	char answer_buff[MAX];
	char* argv[5];
	int argc, channel_id = 0;
	
	pthread_t live_tid, next_tid;
	//sem_t r_mutex;
	//sem_t w_mutex;
	
	client* new_client = malloc(sizeof(client));
	new_client->client_id = c;
	new_client->head = NULL;
	subbed_channel* tmp = NULL;
	subbed_channel *cursor = NULL;

	printf("Client ID is: %d\n", new_client->client_id);

	//sem_init(&r_mutex, 1, 1);
 	sem_init(&mutex, 0, 1);
	 
	struct read_write_struct *read_write = malloc(sizeof(struct read_write_struct));

	//populating the struct with read/write information
	read_write->c = new_client;
	read_write->id = &channel_id;
	read_write->socket = sockfd;
	read_write->memptr = memptr;
	read_write->tid = &live_tid;
	read_write->sig_flag_ptr = &sig_flag;
	read_write->live_flag_ptr = &live_flag;
	read_write->live_ptr = &live;
	read_write->r_size = sizeof(read_buff);
	read_write->w_size = sizeof(answer_buff);
	read_write->next_flag_ptr = &next_flag;
	read_write->w_buff = answer_buff;
	read_write->r_buff = read_buff;

	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_create(&live_tid, &attr, livefeed_thread, read_write);
	printf("Live Thread %lu created.\n", live_tid);

	pthread_attr_t next_attr;
	pthread_attr_init(&next_attr);
	pthread_create(&next_tid, &next_attr, next_thread, read_write);
	printf("Next Thread %lu created.\n", next_tid);

    // infinite loop for chat 
    while (sig_flag) { 

        bzero(read_buff, MAX); 
		bzero(answer_buff, MAX);
		
		strcpy(answer_buff, "Input Not Found.\n");

        // read the message from client and copy it in buffer 
		read(sockfd, read_buff, sizeof(read_buff)); 

		if (strncmp(read_buff, "", 1) == 0) { 
			strcpy(answer_buff, "w");
		}
		//printf("client sent: %s", read_buff);
		
		printf(RED);
			printf("client sent: %s", read_buff);
		printf(RESET);
		//printf("client sent: %s", read_buff);

		// print buffer which contains the client contents 
		argc = parse_input(read_buff, " ", argv);
		
		if(argc >= 2){
			channel_id = is_numeric(argv[1]);
		} else {
			channel_id = -1;
		}

		// if msg contains "Exit" then server exit and chat ended. 
        if (strncmp(argv[0], "BYE", 3) == 0) { 
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
			//next(new_client, channel_id, answer_buff);
			if(new_client->head == NULL){
				char message[32] = "";
				no_subscription(message, answer_buff);
				*read_write->next_flag_ptr = 0;
			} else {
				*read_write->next_flag_ptr = 1;
			}
			
			bzero(answer_buff, MAX);
			bzero(read_buff, MAX);
			continue;
		} else if (strncmp(argv[0],"LIVEFEED", 8) == 0  && !live_flag) {

			//Reading LIVEFEED
			if(new_client->head == NULL){

				//Cant LiveFeed, Answers with N
				strcpy(answer_buff, "N");
				*read_write->live_flag_ptr = 0;
				*read_write->live_ptr = 0;
			} else {

				if(channel_id != -1){

					cursor = search(new_client->head, channel_id);
					
					if(cursor == NULL){
						strcpy(answer_buff, "C");
						*read_write->live_flag_ptr = 0;
						*read_write->live_ptr = 0;
					} else {
						*read_write->live_flag_ptr = 1;
						*read_write->live_ptr = 1;
						strcpy(answer_buff, "R");	
					}

				} else {
				
					//confirming with the client to start livefeed
					*read_write->live_flag_ptr = 1;
					*read_write->live_ptr = 1;

					strcpy(answer_buff, "R");
				}

			}


		} else if (strncmp(argv[0], "SEND", 4) == 0) { 
			
			if(argc != 3){
				wrong_args(answer_buff);
			} else {
				
				// Cant Send a message to -1 ok...
				if(channel_id < 0 || channel_id > 254){
					strcpy(answer_buff, "Please Use Numerical Values 0 to 254.\n");
				} else {
					channel *cur_channel = &memptr->channels[channel_id];

					remove_substring(argv[2], "\n");
					strcpy(cur_channel->posts[cur_channel->post_index++].message, argv[2]);

					char message[25] = {"Client #"};
					cancat_int(message, c);
					strcat(message, ": ");
					strcat(message, read_buff);
					strcpy(answer_buff, "\0");
				}			

			}

		} else if (strncmp(argv[0], "CHANNELS", 8) == 0) { 
			//Reading LIVEFEED

			if(new_client->head == NULL){
				//Cant LiveFeed, Answers with N
				strcpy(answer_buff, "N");
			} else {
				int channel_live = 1;
				subbed_channel* cursor = NULL;
				cursor = new_client->head;	
				char m[32] = "";

				//confirming with the client to start livefeed
				strcpy(answer_buff, "R");

				//sem_post(&w_mutex);
					write(sockfd, answer_buff, sizeof(answer_buff));
				//sem_wait(&w_mutex);

				bzero(answer_buff, MAX);
				bzero(read_buff, MAX);

				while(channel_live && sig_flag){
					//now read, then write
					
					//sem_wait(&r_mutex);
						read(sockfd, read_buff, sizeof(read_buff)); 
					//sem_wait(&r_mutex);
					

					//Handling SIG
					if(strncmp(read_buff, "SIG", 3) == 0){

					}

					//stream read, if stream is broken please stapt after
					if(strncmp(read_buff, "s", 1) == 0){
					} else {
						channel_live = 0;
					}

					if(cursor != NULL){
						
						channel *cur_channel = &memptr->channels[cursor->channel_id];

						bzero(m, sizeof(m)); //clear message buffer
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
						channel_live = 0;
						strcpy(answer_buff, "d");
					}

					//sem_post(&w_mutex);
						write(sockfd, answer_buff, sizeof(answer_buff));
					//sem_wait(&w_mutex);

					bzero(answer_buff, MAX);
					bzero(read_buff, MAX);
				}
				continue;
			}
		} else if (strncmp(argv[0], "STOP", 4) == 0){
			if(live_flag){
				printf("Stopping Livefeed.\n");
				live_flag = 0;
				strcpy(answer_buff, "\0");
			} else {
				strcpy(answer_buff, "\0");
			}
		}

		

			write(sockfd, answer_buff, sizeof(answer_buff));


		bzero(answer_buff, MAX);
		bzero(read_buff, MAX);

    } 



	printf("Closing Client %d...\n", c); 		
	
	clean_up(argv, argc);

	sub_dispose(new_client->head);

	free(read_write);
	free(new_client);

	printf("Threads %lu closing.\n", live_tid);
	pthread_join(live_tid, NULL);
	pthread_join(next_tid, NULL);
	sem_destroy(&mutex);
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
