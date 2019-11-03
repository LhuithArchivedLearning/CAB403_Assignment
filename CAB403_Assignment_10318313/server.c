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
#include "worker.h"

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
volatile sig_atomic_t thread_flag = 1;
volatile sig_atomic_t live_flag = 0;
volatile sig_atomic_t live = 0;

volatile sig_atomic_t next_flag = 0;

void sigint_handler(int sig){
		live_flag = 0;
		sig_flag = 0;
		thread_flag = 0;
		
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
			strcpy(buff, message);
			return;
		}

		//-------------------------------------
		c->head = prepend(c->head, id);
		
		channel *cur_channel = &memptr->channels[id];
		c->head->read_index = cur_channel->post_index;

		cancat_int(message, c->head->channel_id);

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
	worker *w;
	struct memory* memptr;
	int live_id;
	int next_id;
	int socket;

	
	volatile sig_atomic_t *sig_flag_ptr;
	volatile sig_atomic_t *live_flag_ptr;
	volatile sig_atomic_t *live_ptr;
	volatile sig_atomic_t *next_flag_ptr;

	volatile sig_atomic_t *thread_flag_ptr;
};


sem_t mutex;
pthread_mutex_t p_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t schedular_mutex = PTHREAD_MUTEX_INITIALIZER;

//read worker jobs and send them to client
void* poster_thread(void* struct_pass){
	
	struct read_write_struct *read_write = (struct read_write_struct*) struct_pass;

	char w_buff[MAX];
	char m[32] = "";
	job *tmp_job = NULL;

	while(sig_flag && thread_flag) {

		if(tmp_job == NULL && read_write->w->head != NULL){tmp_job = read_write->w->head;} 

	
		if(tmp_job != NULL){
		
			strcpy(m, tmp_job->data);
			//printf("%s", tmp_job->data);
			read_write->w->head = remove_job(read_write->w->head, tmp_job);
			tmp_job = NULL;
		} else {
			strcpy(m, "\0");
		}
			strcpy(w_buff, m);
			write(read_write->socket, w_buff, sizeof(w_buff));
			bzero(w_buff, MAX);
	}

	bzero(w_buff, MAX);
	//free(read_write);
	//free(tmp_job);
	pthread_exit(0);
}

void* livefeed_thread(void* struct_pass){
	
	struct read_write_struct *read_write = (struct read_write_struct*) struct_pass;

	char m[32] = "";

	subbed_channel* cursor;

	while(sig_flag && thread_flag){
		while (*read_write->live_ptr){
			//printf("anus.");
			if(cursor == NULL && read_write->c->head != NULL){ cursor = read_write->c->head;} 
			
				sleep(1);

				//meat and potatoes
				//pthread_mutex_lock(&p_mutex); 

				if(read_write->live_id < 0){
	
					//---------------------------------------- READING ALL ---------------------------
					if(cursor != NULL && read_write->c->head != NULL){

							channel *cur_channel = &read_write->memptr->channels[cursor->channel_id];
						
							if(cursor->read_index < cur_channel->post_index){
								memset(m, 0, sizeof(m)); //clear message buffer
								cancat_int(m, cursor->channel_id);
								strcat(m, ":");

								pthread_mutex_lock(&p_mutex);
									strcat(m, cur_channel->posts[cursor->read_index++].message);
								pthread_mutex_unlock(&p_mutex);
							} else {
								strcpy(m, "\0");
							}

							if(cursor->next != NULL){cursor = cursor->next;} else {cursor = read_write->c->head;}
						
						} else {
							pthread_mutex_lock(&schedular_mutex);
								read_write->w->head = job_prepend(read_write->w->head, 1, "No Channels.");
							pthread_mutex_unlock(&schedular_mutex);
						}
					//---------------------------------------- READING ALL ---------------------------
				} else {
					channel *cur_channel = &memptr->channels[read_write->live_id];
					
					cursor = search(read_write->c->head, read_write->live_id);

					if(cursor->read_index < cur_channel->post_index){
						memset(m, 0, sizeof(m)); //clear message buffer
						cancat_int(m, read_write->live_id);
						strcat(m, ":");
						pthread_mutex_lock(&p_mutex);
							strcat(m, cur_channel->posts[cursor->read_index++].message);
						pthread_mutex_unlock(&p_mutex);
					} else {			
						strcpy(m, "\0");
					}	
				}

				pthread_mutex_lock(&schedular_mutex);
					read_write->w->head = job_prepend(read_write->w->head, 1, m);
				pthread_mutex_unlock(&schedular_mutex);
				//pthread_mutex_unlock(&p_mutex); 
			}
		}

	//free(read_write);
	//free(cursor);
	pthread_exit(0);
}

void* next_thread(void* struct_pass){
	
	struct read_write_struct *read_write = (struct read_write_struct*) struct_pass;

	subbed_channel* cursor = NULL;

	char m[MAX] = "";

	while(sig_flag && thread_flag){
		if(*read_write->next_flag_ptr){

			bzero(m, MAX);
			//sleep(1);
			if(cursor == NULL && read_write->c->head != NULL){
				cursor = read_write->c->head;
			} else {
				*read_write->next_flag_ptr = 0;
				printf("Head is null :(\n");
				break;
			}  

			printf("NEXTING\n");
			

			if(read_write->next_id == -1){
				
				if(read_write->c->head == NULL){
					printf("Cursor is null :(\n");
				} else {
					

					while(1){
						
						if(cursor == NULL){strcpy(m, "no new message."); break;}

						channel *cur_channel = &read_write->memptr->channels[cursor->channel_id];

						if(cursor->read_index < cur_channel->post_index){
							memset(m, 0, sizeof(m)); //clear message buffer
							cancat_int(m, cursor->channel_id);
							strcat(m, ":");
							pthread_mutex_lock(&p_mutex);
								strcat(m, cur_channel->posts[cursor->read_index++].message);
							pthread_mutex_unlock(&p_mutex);
							break;
						} else {
							cursor = cursor->next;
						}
					}
				
				}


			} else {
				
				cursor = search(read_write->c->head, read_write->next_id);

				if(cursor == NULL){
					strcpy(m, "No Channels");
				} else {
					channel *cur_channel = &read_write->memptr->channels[read_write->next_id];

					if(cursor->read_index < cur_channel->post_index){
						memset(m, 0, sizeof(m)); //clear message buffer
						cancat_int(m, cursor->channel_id);
						strcat(m, ":");
						pthread_mutex_lock(&p_mutex);
							strcat(m, cur_channel->posts[cursor->read_index++].message);
						pthread_mutex_unlock(&p_mutex);
					} else {
						strcpy(m, "no new message.");
					}
				}

			}
			pthread_mutex_lock(&schedular_mutex);
				read_write->w->head = job_prepend(read_write->w->head, 1, m);
			pthread_mutex_unlock(&schedular_mutex);

			*read_write->next_flag_ptr = 0;
			cursor = NULL;
		}
	}

	//free(read_write);
	//free(cursor);
	pthread_exit(0);
}
	

void server_chat(int sockfd, int c) { 

    char read_buff[MAX]; 
	char answer_buff[MAX];
	char* argv[5];
	int argc, channel_id = 0, num_jobs = 0;
	
	pthread_t poster_tid, live_tid, next_tid;
	//sem_t r_mutex;
	//sem_t w_mutex;
	
	//-------------- Client Struct -------------------------
	client* new_client = malloc(sizeof(client));
	new_client->client_id = c;
	new_client->head = NULL;
	subbed_channel *cursor = NULL;
	//-------------- Client Struct -------------------------

	//-------------- Worker Struct ------------------------
	worker* new_worker = malloc(sizeof(worker));
	new_worker->head = NULL;
	//-------------- Worker Struct ------------------------

	printf("Client ID is: %d\n", new_client->client_id);

	//sem_init(&r_mutex, 1, 1);
 	//sem_init(&mutex, 0, 1);
	 
	struct read_write_struct *read_write = malloc(sizeof(struct read_write_struct));

	//populating the struct with read/write information
	read_write->c = new_client;
	read_write->w = new_worker;
	
	read_write->socket = sockfd;
	read_write->memptr = memptr;
	read_write->sig_flag_ptr = &sig_flag;
	read_write->live_flag_ptr = &live_flag;
	read_write->live_ptr = &live;
	read_write->next_flag_ptr = &next_flag;	
	read_write->thread_flag_ptr = &thread_flag;

	pthread_attr_t live_attr;
	pthread_attr_init(&live_attr);
	pthread_create(&live_tid, &live_attr, livefeed_thread, read_write);
	printf("Live Thread %lu created.\n", live_tid);

	pthread_attr_t next_attr;
	pthread_attr_init(&next_attr);
	pthread_create(&next_tid, &next_attr, next_thread, read_write);
	printf("Next Thread %lu created.\n", next_tid);

	pthread_attr_t post_attr;
	pthread_attr_init(&post_attr);
	pthread_create(&poster_tid, &post_attr, poster_thread, read_write);
	printf("Next Thread %lu created.\n", poster_tid);

    // infinite loop for chat 
    while (sig_flag) { 

        bzero(read_buff, MAX); 
		bzero(answer_buff, MAX);
		
        // read the message from client and copy it in buffer 
		read(sockfd, read_buff, sizeof(read_buff)); 

		strcpy(answer_buff, "Input Not Found.");

		printf(RED);
			printf("client sent: %s", read_buff);
		printf(RESET);


		// print buffer which contains the client contents 
		argc = parse_input(read_buff, " ", argv);
		//printf("%d", argc);
		if(argc >= 2){
			channel_id = is_numeric(argv[1]);
		} else {
			channel_id = -1;
		}

		// if msg contains "Exit" then server exit and chat ended. 
        if (strncmp(argv[0], "BYE", 3) == 0) { 
			thread_flag = 0;
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
		} else if (strncmp(argv[0], "NEXT", 4) == 0 && !*read_write->next_flag_ptr) { 
			
			subbed_channel *next_cursor = NULL;

			//Reading LIVEFEED
			if(new_client->head == NULL){
				*read_write->next_flag_ptr = 0;
				printf("????");
				strcpy(answer_buff, "No Channels.");
			} else {
			
				read_write->next_id = channel_id;
				
				if(channel_id != -1){
				
					cursor = search(new_client->head, channel_id);

					if(cursor == NULL){
				
						*read_write->next_flag_ptr = 0;
						strcpy(answer_buff, "not subbed to channel");
					} else {
						*read_write->next_flag_ptr = 1;
						strcpy(answer_buff, "\0");
					}
				} else {
					*read_write->next_flag_ptr = 1;
					strcpy(answer_buff, "\0");
				}

			}
			
			free(next_cursor);
		} else if (strncmp(argv[0],"LIVEFEED", 8) == 0  && !live_flag) {

			//Reading LIVEFEED
			if(new_client->head == NULL){
				*read_write->live_flag_ptr = 0;
				*read_write->live_ptr = 0;
		
				strcpy(answer_buff, "No Channels.");
			} else {
			
				if(channel_id != -1){
				
					cursor = search(new_client->head, channel_id);

					if(cursor == NULL){
						*read_write->live_flag_ptr = 0;
						*read_write->live_ptr = 0;
						strcpy(answer_buff, "No Channels");
					} else {
						*read_write->live_flag_ptr = 1;
						*read_write->live_ptr = 1;
						
						strcpy(answer_buff, "GOING LIVE WITH A CHANNELS!");
					}

				} else {
	
					*read_write->live_flag_ptr = 1;
					*read_write->live_ptr = 1;
					strcpy(answer_buff, "GOING LIVE!.");
				}

				read_write->live_id = channel_id;
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

					pthread_mutex_lock(&p_mutex);
						strcpy(cur_channel->posts[cur_channel->post_index++].message, argv[2]);
					pthread_mutex_unlock(&p_mutex);

					char message[25] = {"Client #"};
					cancat_int(message, c);
					strcat(message, ": ");
					strcat(message, read_buff);
					strcpy(answer_buff, "\0");
				}			

			}

		} else if (strncmp(argv[0], "CHANNELS", 8) == 0) { 
			//Reading LIVEFEED
			char m[MAX] = "";
			

			if(new_client->head == NULL){
				strcpy(answer_buff, "No Channels");
			} else {
				int channel_live = 1;
				cursor = new_client->head;

				while(channel_live){
					
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
						cursor = cursor->next;

					} else { 
						channel_live = 0;
						break;
					}
					//sem_wait(&mutex);
					pthread_mutex_lock(&schedular_mutex);
						new_worker->head = job_prepend(new_worker->head, 1, m);
					pthread_mutex_unlock(&schedular_mutex);
					//sem_post(&mutex);
				}

				continue;
			}
		} else if (strncmp(argv[0], "STOP", 4) == 0){
			if(live_flag){
				*read_write->live_flag_ptr = 0;
				*read_write->live_ptr = 0;
				strcpy(answer_buff, "Stopping Livefeed.");
			} else {
				//strcpy(answer_buff, "\0");
			}
		} 

		if(strncmp(argv[0], " ", 1) == 0){strcpy(answer_buff, "\0");}

		pthread_mutex_lock(&schedular_mutex);
			new_worker->head = job_prepend(new_worker->head, 1, answer_buff);
		pthread_mutex_unlock(&schedular_mutex);

		//bzero(answer_buff, MAX);
		bzero(read_buff, MAX);
    } 



	bzero(read_buff, MAX);

	printf("Closing Client %d...\n", c); 		
	
	clean_up(argv, argc);

	sub_dispose(new_client->head);
	job_dispose(new_worker->head);

	free(read_write);
	free(new_client);
	free(new_worker);
	free(cursor);

	printf("Threads %lu closing.\n", poster_tid);

	pthread_join(poster_tid, NULL);
	pthread_join(live_tid, NULL);
	pthread_join(next_tid, NULL);

	//sem_destroy(&mutex);
	pthread_mutex_destroy(&p_mutex);
	pthread_mutex_destroy(&schedular_mutex);
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

	if(argc < 2){
		fprintf(stderr,"using defualt port: 10000\n");
		port = 10000;
		//exit(1);
	} else {
		//port = atoi(argv[2]);
		if(atoi(argv[1]) == 0){perror("port"); exit(1);}
		port = atoi(argv[1]);
	}


	
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
