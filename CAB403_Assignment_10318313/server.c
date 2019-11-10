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
#define BACKLOG 10     /* how many pending connections queue will hold */
#define MAX 1250 

int sockfd, new_fd;
callback display_channels = display;

volatile int pid_test = 0;
volatile int global_socket = 0;

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

pthread_mutex_t p_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t schedular_mutex = PTHREAD_MUTEX_INITIALIZER;


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

void sigint_handler(int sig){
		live_flag = 0;
		live = 0;
		sig_flag = 0;
		next_flag = 0;
		thread_flag = 0;
		
		printf("pid:%d\n", getpid());

		
		char m[3] = "SIG";
		write(global_socket, m, sizeof(m));

		if(pid_test == 0){clean_up_shared_mem();}
}

struct memory* memptr;

//add to poster queue
void add_to_queue(worker* w, char* m){
	pthread_mutex_lock(&schedular_mutex);
		w->head = job_prepend(w->head, 1, m);
	pthread_mutex_unlock(&schedular_mutex);
}

void invalid_channel(worker* w, int id){
	char m[MAX] = {"Invalid channel:"};
	cancat_int(m, id);
	add_to_queue(w, m);
}

void not_subbed(worker* w, int id){
	char m[MAX] = {"Not subscribed to channel "};
	cancat_int(m, id);
	add_to_queue(w, m);
}

void no_subscription(worker* w){
	add_to_queue(w, "Not subscribed to any channels.");
}

void wrong_args(worker* w){
	add_to_queue(w, "Wrong Number of Arguments.");
}

void wrong_values(worker* w){
	add_to_queue(w, "Please Use Values 0 - 255");
}

void subscribe(client* c, worker* w, int id){
		subbed_channel* sub_tmp = NULL;

		//Passing Subbing information 
		//-------------------------------------
		sub_tmp = search(c->head, id);

		if(sub_tmp != NULL){
			char message[MAX] = {"Already Subscribed to "};
			cancat_int(message, id);
			add_to_queue(w, message);
		} else {

			char message[MAX] = {"Subscribed to channel "};

			//-------------------------------------
			c->head = prepend(c->head, id);
			
			channel *cur_channel = &memptr->channels[id];
			c->head->read_index = cur_channel->post_index;

			cancat_int(message, c->head->channel_id);
			add_to_queue(w, message);	
		}
}

void unsubscribe(client* c, worker* w, int id){

	subbed_channel* sub_tmp = NULL;

	sub_tmp = search(c->head, id);

	if(sub_tmp == NULL){
		not_subbed(w, id);
	} else {
		char m[MAX] = {"Unsubscribing from "};

		subbed_channel* checker = c->head;

		if(c->head->next == NULL){
			if(live_flag){
				live_flag = 0;
				live = 0;
			}
		}

		if((c->head = remove_sub(c->head, sub_tmp)) == NULL && live_flag){
		}		

		cancat_int(m, id);
		add_to_queue(w, m);
	}
}

void send_to(client* c, worker* w, int id, char* arg){
	
	char m[MAX] = "";

		// Cant Send a message to -1 ok...
		if(id < 0 || id > 254){
			add_to_queue(w, "Please Use Numerical Values 0 to 254.");
		} else {
			channel *cur_channel = &memptr->channels[id];

			remove_substring(arg, "\n");

			pthread_mutex_lock(&p_mutex);
				sem_wait(&cur_channel->mutex);
					strcpy(cur_channel->posts[cur_channel->post_index++].message, arg);
				sem_post(&cur_channel->mutex);
			pthread_mutex_unlock(&p_mutex);

			//add_to_queue(w, "SENT");
		}			
}

void channels(client* c, worker* w, int id){

	//Reading LIVEFEED
	char m[MAX] = "";

	int channel_live = 1;
	subbed_channel* cursor = c->head;

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

		add_to_queue(w, m);
	}

		//continue;


}

//read worker jobs and send them to client
void* poster_thread(void* struct_pass){
	
	struct read_write_struct *read_write = (struct read_write_struct*) struct_pass;

	char w_buff[MAX];
	char m[MAX] = "";

	while(sig_flag && thread_flag) {

		if(read_write->w->head != NULL){	
			pthread_mutex_lock(&schedular_mutex);
				memset(m, 0, sizeof(m));
				strcpy(m, read_write->w->head->data);
				printf("%s\n", m);
				read_write->w->head = job_remove_front(read_write->w->head);
			pthread_mutex_unlock(&schedular_mutex);
		} else {
			strcpy(m, "\0");
		}
		 
		strcpy(w_buff, m);
		write(read_write->socket, w_buff, sizeof(w_buff));

		bzero(w_buff, MAX);
	}

	printf("Closing Poster.\n");
	bzero(w_buff, MAX);
	pthread_exit(0);
}

void* livefeed_thread(void* struct_pass){
	
	struct read_write_struct *read_write = (struct read_write_struct*) struct_pass;
	char m[MAX] = "";

	subbed_channel* cursor = NULL;

	while(sig_flag && thread_flag){
		while (*read_write->live_ptr && thread_flag){
			if(read_write->c->head == NULL){printf("no channels.\n"); break;}

			if(cursor == NULL && read_write->c->head != NULL){ cursor = read_write->c->head;} 
			
				//sleep(1);

				//meat and potatoes
				if(read_write->live_id == -1){
					//---------------------------------------- READING ALL ---------------------------
					if(cursor != NULL && read_write->c->head != NULL){
							channel *cur_channel = &read_write->memptr->channels[cursor->channel_id];
						
							if(cursor->read_index < cur_channel->post_index){
								memset(m, 0, sizeof(m)); //clear message buffer
								cancat_int(m, cursor->channel_id);
								strcat(m, ":");

								pthread_mutex_lock(&p_mutex);
									sem_wait(&cur_channel->mutex);
										strcat(m, cur_channel->posts[cursor->read_index++].message);
										pthread_mutex_lock(&schedular_mutex);
											read_write->w->head = job_prepend(read_write->w->head, 1, m);
										pthread_mutex_unlock(&schedular_mutex);		
									sem_post(&cur_channel->mutex);
								pthread_mutex_unlock(&p_mutex);

	
							} else {
								//strcpy(m, "\0");

								if(cursor->next != NULL){cursor = cursor->next;} 
								else {cursor = read_write->c->head;}
							}
						
						} else {
							no_subscription(read_write->w);
							break;
		
							//read_write->w->head = job_prepend(read_write->w->head, 1, "No Channels.");
						}
					//---------------------------------------- READING ALL ---------------------------
				} else {
					//---------------------------------------- READING CHANNEL ---------------------------
					if(cursor != NULL && read_write->c->head != NULL){
						channel *cur_channel = &memptr->channels[read_write->live_id];
						
						cursor = search(read_write->c->head, read_write->live_id);

						if(cursor->read_index < cur_channel->post_index){
							memset(m, 0, sizeof(m)); //clear message buffer
							cancat_int(m, read_write->live_id);
							strcat(m, ":");

							pthread_mutex_lock(&p_mutex);
								sem_wait(&cur_channel->mutex);
									strcat(m, cur_channel->posts[cursor->read_index++].message);
									pthread_mutex_lock(&schedular_mutex);
										read_write->w->head = job_prepend(read_write->w->head, 1, m);
									pthread_mutex_unlock(&schedular_mutex);
								sem_post(&cur_channel->mutex);
							pthread_mutex_unlock(&p_mutex);

						} else {			
						}	
					} else {
						no_subscription(read_write->w);
						break;
						
						//read_write->w->head = job_prepend(read_write->w->head, 1, "No Channels.");

					}
					//---------------------------------------- READING CHANNEL ---------------------------
				}

			}

			if(cursor != NULL){ 
				pthread_mutex_lock(&schedular_mutex);
					read_write->w->head = job_prepend(read_write->w->head, 1, "exiting livefeed.");
					cursor = NULL;
				pthread_mutex_unlock(&schedular_mutex);
			}
		}

	printf("Closing Live.\n");
	pthread_exit(0);
}

void* next_thread(void* struct_pass){
	
	struct read_write_struct *read_write = (struct read_write_struct*) struct_pass;

	subbed_channel* cursor = NULL;

	char m[MAX] = "";

	while(sig_flag && thread_flag){
		if(*read_write->next_flag_ptr && thread_flag){

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
						if(cursor == NULL){
							
							pthread_mutex_lock(&schedular_mutex);
								read_write->w->head = job_prepend(read_write->w->head, 1, "No New Messages");
							pthread_mutex_unlock(&schedular_mutex);

							 break;
							 }

						channel *cur_channel = &read_write->memptr->channels[cursor->channel_id];

						if(cursor->read_index < cur_channel->post_index){
							memset(m, 0, sizeof(m)); //clear message buffer
							cancat_int(m, cursor->channel_id);
							strcat(m, ":");
							
							pthread_mutex_lock(&p_mutex);
								sem_wait(&cur_channel->mutex);
									strcat(m, cur_channel->posts[cursor->read_index++].message);

									pthread_mutex_lock(&schedular_mutex);
										read_write->w->head = job_prepend(read_write->w->head, 1, m);
									pthread_mutex_unlock(&schedular_mutex);

								sem_post(&cur_channel->mutex);
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
							sem_wait(&cur_channel->mutex);
								strcat(m, cur_channel->posts[cursor->read_index++].message);

								pthread_mutex_lock(&schedular_mutex);
									read_write->w->head = job_prepend(read_write->w->head, 1, m);
								pthread_mutex_unlock(&schedular_mutex);

							sem_post(&cur_channel->mutex);
						pthread_mutex_unlock(&p_mutex);
					} else {
						pthread_mutex_lock(&schedular_mutex);
							read_write->w->head = job_prepend(read_write->w->head, 1, "No New Messages");
						pthread_mutex_unlock(&schedular_mutex);
					}
				}
			}
			//read_write->w->head = job_prepend(read_write->w->head, 1, m);

			*read_write->next_flag_ptr = 0;
			cursor = NULL;
		}
	}

	printf("Closing Next.\n");
	pthread_exit(0);
}
	

void server_chat(int sockfd, int c) { 
	
	global_socket = sockfd;

    char read_buff[MAX]; 
	char answer_buff[MAX];
	char* argv[5];
	int argc, channel_id = 0, num_jobs = 0;
	
	pthread_t poster_tid, live_tid, next_tid;
	
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
		
        // read the message from client and copy it in buffer,
		// if connection lost before the client can say bye
		// close the chat 
		if(read(sockfd, read_buff, sizeof(read_buff)) == -1){
			thread_flag = 0;
			live_flag = 0; 
			live = 0;
			next_flag = 0;
			sig_flag = 0;
			printf("Connection lost, rude client close :(.\n");
			break;
		} 


		printf(RED);
			printf("client sent: %s", read_buff);
		printf(RESET);


		// print buffer which contains the client contents 
		argc = parse_input(read_buff, " ", argv);


		//Channel ID flags, -1 means no channel, -2 means invalid channel
		if(argc >= 2){
			channel_id = is_numeric(argv[1]);

			if(channel_id > 254 || channel_id < 0){
				channel_id = -2;
			}
		} else {
			channel_id = -1;
		}

		//remove sig charcters and other nasties
		string_remove_nonalpha(argv[0]);

		// if msg contains "BYE" then server exit and chat ended. 
        if (strncmp(argv[0], "BYE", 3) == 0) { 
			thread_flag = 0;
			break; 
        } else if (strncmp(argv[0], "SUB", 3) == 0) { 
			if(argc < 2){
				wrong_args(new_worker); 
			} else {
				if(channel_id == -2){
					add_to_queue(new_worker, "Please Use Values 0 - 255");
				} else{
					subscribe(new_client, new_worker, channel_id);
				}				
			}
		} else if (strncmp(argv[0], "UNSUB", 5) == 0) { 
			
			if(new_client->head == NULL){ 
				no_subscription(new_worker);
			} else {

				if(argc < 2){
					wrong_args(new_worker); 
				} else {

					if(channel_id == -2){
						add_to_queue(new_worker, "Please Use Values 0 - 255");
					} else {
						
						subbed_channel* tmp_check = search(new_client->head, channel_id);

						if(tmp_check == NULL){
							not_subbed(new_worker, channel_id);
						} else {
							unsubscribe(new_client, new_worker, channel_id);
						}

					}				
				}
			}
			
		} else if (strncmp(argv[0], "NEXT", 4) == 0 && !*read_write->next_flag_ptr) { 
			
			subbed_channel *next_cursor = NULL;

			//Reading LIVEFEED
			if(new_client->head == NULL){
				*read_write->next_flag_ptr = 0;
				no_subscription(new_worker);
			} else {
			
				read_write->next_id = channel_id;
				
				if(channel_id == -1){
					//Next all
					*read_write->next_flag_ptr = 1;
					add_to_queue(new_worker, "\0");
				} else {
					
					if(channel_id != -2){ 
						cursor = search(new_client->head, channel_id);

						if(cursor == NULL){
							*read_write->next_flag_ptr = 0;
							not_subbed(new_worker, channel_id);
						} else {
							*read_write->next_flag_ptr = 1;
						}

					} else {
						wrong_values(new_worker);
					}
				}
			}
			
			free(next_cursor);
		} else if (strncmp(argv[0],"LIVEFEED", 8) == 0) {

			if(!live_flag){
				if(new_client->head == NULL){
					live = 0;
					live_flag = 0;
					no_subscription(new_worker);
				} else {
							
					read_write->live_id = channel_id;

					if(channel_id == -1){
						
						live_flag = 1;
						live = 1;
						add_to_queue(new_worker, "Entering Livefeeed.");
					} else {
						
						if (channel_id != -2){
							cursor = search(new_client->head, channel_id);
							
							if(cursor == NULL){
								live = 0;
								live_flag = 0;
								not_subbed(new_worker, channel_id);
							} else {
								live_flag = 1;
								live = 1;

								char m[MAX] = "Entering Livefeeed: ";
								cancat_int(m, channel_id);
								add_to_queue(new_worker, m);
							}
						} else {
							wrong_values(new_worker);
						}


					} 

				}
			} else {
				add_to_queue(new_worker, "Livefeed already active.");
			}


		} else if (strncmp(argv[0], "SEND", 4) == 0) { 

			if(argc != 3){
				wrong_args(new_worker);
			} else {
				if(channel_id == -2){
					wrong_values(new_worker);
				} else {
					//1024 - 1 for \0 terminator
					if(strlen(argv[2]) > 1024){
						add_to_queue(new_worker, "Message Limit Exceeded 0 - 1023");
					} else {
						send_to(new_client, new_worker, channel_id, argv[2]);
					}

				}
			}
			
		} else if (strncmp(argv[0], "CHANNELS", 8) == 0) {
			
			if(new_client->head == NULL){
				no_subscription(new_worker);
			} else {
				channels(new_client, new_worker, channel_id);
			}
		} else if (strncmp(argv[0], "STOP", 4) == 0){
			if(live_flag){
				live = 0;
				live_flag = 0;
				//add_to_queue(new_worker, "Stopping Livefeed.");
			} else {
				add_to_queue(new_worker, "Livefeed not active.");
			}
		} else if (strncmp(argv[0], "SIG", 3) == 0) { 
			if(live_flag) {
				live_flag = 0; 
				live = 0;
				//add_to_queue(new_worker, "Stopping Livefeed.");
			} else {
				printf("Stopping here?");
				break; 
			}
        } else { add_to_queue(new_worker, "Invalid Command."); }

		bzero(read_buff, MAX);
    } 

	thread_flag = 0;
	live_flag = 0; 
	live = 0;
	next_flag = 0;


	bzero(read_buff, MAX);

	printf("Closing Client %d...\n", c); 		
	
	clean_up(argv, argc);

	sub_dispose(new_client->head);
	job_dispose(new_worker->head);

	free(read_write);
	free(new_client);
	free(new_worker);

	printf("Threads %lu closing.\n", poster_tid);

	pthread_join(poster_tid, NULL);
	pthread_join(live_tid, NULL);
	pthread_join(next_tid, NULL);


	pthread_mutex_destroy(&schedular_mutex);
	pthread_mutex_destroy(&p_mutex);
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
		fprintf(stderr,"using defualt port: 12345\n");
		port = 12345;
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

			pthread_mutex_lock(&p_mutex);
				memptr->num_clients++;
			pthread_mutex_unlock(&p_mutex);

			int clientid = getpid();

			cancat_int(message, clientid);
			strcat(message, ">\n");
			size_t len = strlen(message);
			/* WELCOME MESSAGE */	
			
			if (send(new_fd, message, len, 0) == -1){ perror("send");}

			//chat with the client
			server_chat(new_fd, clientid);
			
			pthread_mutex_lock(&p_mutex);
				memptr->num_clients--;
			pthread_mutex_unlock(&p_mutex);

			//removing pthread mutex
			pthread_mutex_destroy(&p_mutex);
			printf("server: closing connection from %s\n", inet_ntoa(their_addr.sin_addr));

			close(new_fd);

			exit(0);
		}

		close(new_fd);  /* parent doesn't need this */

		while(waitpid(-1, NULL,WNOHANG) > 0){}; /* clean up child processes */
	}

	return 0;		
}
