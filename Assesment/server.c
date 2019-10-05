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

//#define MYPORT 54321    /* the port users will be connecting to */
#define BACKLOG 10     /* how many pending connections queue will hold */
#define MAX 80 
int sockfd, new_fd;

void concactInt(char m[] , int value){
		char ID_str[4];
		snprintf(ID_str, sizeof ID_str, "%d", value);
		strcat(m, ID_str);
}

int extractDigit(char m[]){
	char *p = m;

	while(*p){
		if(isdigit(*p) || ((*p == '-' || *p=='+') && isdigit(*(p + 1)))){
			return strtol(p, &p, 10); //read number;
		} else {
			p++;
		}
	}
}

char* removeSubString(char* string, const char* sub){
	char *match = string;

	size_t len = strlen(sub);

	if(len > 0){
		char *p = string;
		while((p = strstr(p, sub)) != NULL){
			memmove(p, p + len, strlen(p + len) + 1);
		}
	} 

	return string;
}

char* remove_spaces(char* s){
	const char* d = s;

	do{
		while(*d == ' '){
			++d;
		}
	} while (*s++ = *d++);
}

int parse_input(char* arg, char* seperator, char** arg_array){
	char* str, *saveptr, *token; 
	int j;

	for(j = 1, str = arg ; ; j++, str = NULL){
		
		//stop seperations after arguments go past 2, for messages
		if(j > 2){
			token = strtok_r(str, "", &saveptr);
		} else {
			token = strtok_r(str, seperator, &saveptr);
		}

		if(token == NULL){break;}

		arg_array[j - 1] = malloc(sizeof(token));
		strcpy(arg_array[j - 1], token);
	}

	return j - 1;
}

void clean_up(char ** arg_array, int argc){
	printf("Cleaning up...\n");
	for(int i = 0; i < argc; i++){free(arg_array[i]);}
}

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

	//printf("pid:%d AHHH SIGINT!\n", getpid());
	if(pid_test == 0){clean_up_shared_mem();}

	//exit(0);
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

void subscribe(client* c, int id, char buff[]){
		subbed_channel* sub_tmp = NULL;
		//Passing Subbing information 
		//-------------------------------------
		char message[32] = {"Subscribed to channel "};

		sub_tmp = search(c->head, id);

		if(sub_tmp != NULL){
			strcpy(message, "Already Subscribed to ");
			concactInt(message, c->head->channel_id);
			strcat(message, "\n");
			strcpy(buff, message);
			return;
		}

		//-------------------------------------
		c->head = prepend(c->head, id);
		
		channel *cur_channel = &memptr->channels[id];
		c->head->read_index = cur_channel->post_index;

		concactInt(message, c->head->channel_id);

		strcat(message, "\n");
		strcpy(buff, message);
}

void unsubscribe(client* c, int id, char buff[]){

	subbed_channel* sub_tmp = NULL;
	callback display_channels = display;
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

	concactInt(message, id);

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
		concactInt(message, id);
	} else {
		channel *cur_channel = &memptr->channels[id];
		
		concactInt(message, id);
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

void livefeed(client* c, int id, char buff[]){

	char message[32] = {"GOING LIVE!"};

	if(c->head == NULL){
		no_subscription(message, buff);
		return;
	} 

	strcat(message, "\n");
	strcpy(buff, message);
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
        read(sockfd, read_buff, sizeof(read_buff)); 
        
		// print buffer which contains the client contents 
		argc = parse_input(read_buff, " ", argv);
		
		//for(int i = 0; i < argc; i++) {
		//	//if(argv[i] == NULL){break;}
		//
		//	//if(i == 0){printf("From Client: ");}
		//	//printf("arg %d: %s ", i, argv[i]);
		//}

		if(argc > 1){ channel_id = atoi(argv[1]);}

		// if msg contains "Exit" then server exit and chat ended. 
        if (strncmp("BYE", argv[0], 3) == 0) { 
           
		    printf("Closing Client <%d>...\n", c); 		
			
			clean_up(argv, argc);

			sub_dispose(new_client->head);
			free(new_client);
            
			break; 
        } else if (strncmp(argv[0], "SUB", 3) == 0) { 

			if(argc < 2){
				wrong_args(answer_buff); 
			} else {				
				subscribe(new_client, channel_id, answer_buff);
			}

			
		} else if (strncmp(argv[0], "UNSUB", 5) == 0) { 
			unsubscribe(new_client, channel_id, answer_buff);
		} else if (strncmp(argv[0], "NEXT", 4) == 0) { 
			next(new_client, channel_id, answer_buff);
		} else if (strncmp(argv[0],"LIVEFEED", 8) == 0) {
			livefeed(new_client, channel_id, answer_buff);
		} else if (strncmp(argv[0], "SEND", 4) == 0) { 
			
			if(argc > 2){
			
			channel *cur_channel = &memptr->channels[channel_id];
			
			removeSubString(argv[2], "\n");
			strcpy(cur_channel->posts[cur_channel->post_index++].message, argv[2]);

			char message[25] = {"Client #"};
			concactInt(message, c);
			strcat(message, ": ");

			strcat(message, read_buff);

			//strcpy(memptr->channels[0].posts[0].message, message);
			strcpy(answer_buff, "Message Sent to : ");
			concactInt(answer_buff,  channel_id);
			strcat(answer_buff, "\n");
			} else {
				strcpy(answer_buff, "Missing Arguments.\n");
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
			concactInt(message, clientid);
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
