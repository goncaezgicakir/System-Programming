//GONCA EZGI CAKIR 
//151044054
//CSE344 FINAL PROJECT - BibakBOXClient Header


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#define MAX_DIR_PATH 128
#define BUFFER_SIZE 2048
#define MAX_LISTEN 10
#define MAX_FILE 256
#define MAX_IP_LENGHT 30
#define MAX_PATH_LENGTH 257
#define MAX_NAME_LENGTH 64
#define MAX_LOG_LENGTH 128

#define DEFAULT_VALUE "X"

#include "BibakBOXHelper.h"


//globals
static volatile sig_atomic_t keepRunning;
int removeControl;


char dirName[MAX_DIR_PATH];                      //client's directory (USER ARGUMENT)
ClientInfo client;                              //client information that is going to be sent to server side

int clientSocket;
char clientIP[INET_ADDRSTRLEN];
struct sockaddr_in clientAddress;

int serverSocket;
int serverPort;                     //server machien socket listening port (USER ARGUMENT)
char serverIP[INET_ADDRSTRLEN];     //server machine IP address (USER ARGUMENT)
struct sockaddr_in serverAddress;

char* log_filename;
int log_fd;                   //log file fd
pthread_mutex_t logfile_mutex = PTHREAD_MUTEX_INITIALIZER;


//method declarations
int getArguments(int argc, char **argv);
void signal_handler(int signal);
void freeResources();
char* getLocalIP();




