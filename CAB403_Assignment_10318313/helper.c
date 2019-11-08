#include <stdio.h> 
#include <stdlib.h> 
#include <ctype.h>
#include <string.h> 

void cancat_int(char m[] , int value){
		char ID_str[4];
		snprintf(ID_str, sizeof ID_str, "%d", value);
		strcat(m, ID_str);
}

int extract_digit(char m[]){
	char *p = m;

	while(*p){
		if(isdigit(*p) || ((*p == '-' || *p=='+') && isdigit(*(p + 1)))){
			return strtol(p, &p, 10); //read number;
		} else {
			p++;
		}
	}
}

void string_remove_nonalpha(char *string){
	unsigned long i = 0;
	unsigned long j = 0;
	char c;

	while((c = string[i++]) != '\0'){
		if(isalnum(c)){
			string[j++] = c;
		}
	}

	string[j] = '\0';
}

char* remove_substring(char* string, const char* sub){
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

int is_numeric(const char * s){
	
	if (s == NULL || *s == '\0' || isspace(*s)){
		return -1;
	}

    
	for(int i = 0; i < strlen(s) - 1; i++){
		
		if(isdigit(s[i]) == 0){
			return -1;
		}
	}
	
	return atoi(s);
}