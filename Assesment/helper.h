#ifndef HELP_H
#define HELP_H

#include <stdio.h> 
#include <stdlib.h> 
#include <ctype.h>
#include <string.h> 

void cancat_int(char m[] , int value);
int extract_digit(char m[]);
char* remove_substring(char* string, const char* sub);
char* remove_spaces(char* s);
int parse_input(char* arg, char* seperator, char** arg_array);
void clean_up(char ** arg_array, int argc);
int is_numeric(const char * s);

#endif