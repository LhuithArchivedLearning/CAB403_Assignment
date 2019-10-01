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

#include "shared.h"

//#define MYPORT 54321    /* the port users will be connecting to */
#define BACKLOG 10     /* how many pending connections queue will hold */
#define MAX 80 

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

void server_chat(int sockfd, int c) { 
    char read_buff[MAX]; 
	char answer_buff[MAX];

    //int n; 
    // infinite loop for chat 
    for (;;) { 
        bzero(read_buff, MAX); 
		bzero(answer_buff, MAX);
		
		strcpy(answer_buff, "Input Not Found.\n");

        // read the message from client and copy it in buffer 
        read(sockfd, read_buff, sizeof(read_buff)); 
        // print buffer which contains the client contents 
        printf("From client: %s", read_buff); 

		// if msg contains "Exit" then server exit and chat ended. 
        if (strncmp("BYE", read_buff, 3) == 0) { 
            //printf("Server Closing Client Socket...\n"); 
            break; 
        } else if (strncmp("SUB <", read_buff, 5) == 0) { 

			//Passing Subbing information 
			//-------------------------------------
			char message[255] = {"SUBBED TO CHANNEL <"};

			//grab channel
			int channelid = extractDigit(read_buff);

			concactInt(message, channelid);

			strcat(message, ">\n");
			strcpy(answer_buff, message);
			//-------------------------------------

		} else if (strncmp("UNSUB", read_buff, 5) == 0) { 
			strcpy(answer_buff, "UNSUBBING!.\n");
		} else if (strncmp("NEXT <", read_buff, 6) == 0) { 

			//Passing Subbing information 
			//-------------------------------------
			char message[255] = {"NEXT UNDREAD <"};

			//grab channel
			int channelid = extractDigit(read_buff);

			concactInt(message, channelid);

			strcat(message, ">\n");
			strcpy(answer_buff, message);

			strcpy(answer_buff, message);

		} else if (strncmp("LIVEFEED<1>", read_buff, 11) == 0) { 
			strcpy(answer_buff, "LIVEFEED!.\n");
		} else if (strncmp("LIVEFEED", read_buff, 8) == 0) { 
			strcpy(answer_buff, "LIVEFEED!.\n");
		} else if (strncmp("NEXT", read_buff, 4) == 0) { 
			strcpy(answer_buff, "NEXT.\n");
		} else if (strncmp("SEND", read_buff, 4) == 0) { 
			strcpy(answer_buff, "NEXT.\n");
		} 

		write(sockfd, answer_buff, sizeof(answer_buff));
		bzero(answer_buff, MAX);
		bzero(read_buff, MAX);
    } 
} 


int main(int argc, char *argv[]){
	int sockfd, new_fd, port;  /* listen on sock_fd, new connection on new_fd */
	struct sockaddr_in my_addr;    /* my address information */
	struct sockaddr_in their_addr; /* connector's address information */
	socklen_t sin_size;

	//init_shared();

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

	printf("server startsss listnening ...\n"); 

	/* repeat: accept, send, close the connection */
	/* for every accepted connection, use a sepetate process or thread to serve it */
	while(1) {  /* main accept() loop */
		sin_size = sizeof(struct sockaddr_in);
		if ((new_fd = accept(sockfd, (struct sockaddr *)&their_addr, \
		&sin_size)) == -1) {
			perror("accept");
			continue;
		}

		printf("server: got connection from %s\n", \
			inet_ntoa(their_addr.sin_addr));
		if (!fork()) { /* this is the child process */

		
			char message[255] = {"Welcome! Your Client ID is <"};

			concactInt(message, getpid());
			strcat(message, ">\n");
			size_t len = strlen(message);

			if (send(new_fd, message, len, 0) == -1){ perror("send");}

			//chat with the client
			server_chat(new_fd, 1);
							printf("server: closing connection from %s\n", \
			inet_ntoa(their_addr.sin_addr));

			close(new_fd);

			exit(0);
		}

		close(new_fd);  /* parent doesn't need this */

		while(waitpid(-1,NULL,WNOHANG) > 0); /* clean up child processes */
	}

	clear_shared();

	return 0;		
}
