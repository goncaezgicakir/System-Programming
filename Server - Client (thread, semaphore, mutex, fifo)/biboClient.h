//GONCA EZGI CAKIR 
//151044054
//CSE344 MIDTERM PROJECT - biboClient Header


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

#define MAX_REQUEST_LENGTH 1024

//bibo client information struct
struct BiboClient{
    char serverPid[30];
    char connectInfo[30];   //ty connoct/connect
    char clientPid[30];
    char clientId[30];  //client01,client02..etc
};



//method declarations
int getArguments(int argc, char **argv, int pid);
void connectServerFifo();
void signal_handler(int signal);
void sendRequest();
void freeResources();



