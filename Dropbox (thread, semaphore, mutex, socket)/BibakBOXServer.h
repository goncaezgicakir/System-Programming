//GONCA EZGI CAKIR 
//151044054
//CSE344 FINAL PROJECT - BibakBOXServer Header


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <errno.h>
#include <semaphore.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>


#define MAX_DIR_PATH 128
#define BUFFER_SIZE 2048
#define QUEUE_SIZE 1024
#define MAX_IP_LENGHT 30
#define MAX_FILE 256
#define MAX_PATH_LENGTH 257
#define MAX_NAME_LENGTH 64

#define DEFAULT_PORT 8080
#define DEFAULT_VALUE "X"

#include "BibakBOXHelper.h"



//globals
static volatile sig_atomic_t keepRunning;
int removeControl;

int currentSize;

char directory[MAX_DIR_PATH];   //server directory
int poolSize;                   //thread pool size
int serverPort;                       //port number

int serverSocket;               //server socket fd
struct sockaddr_in serverAddress;

FILE* tempFile;                 //server double int checker fd
ClientInfo* clientQueue;        //client queue
pthread_t* tids;                //thread ids
pid_t* client_pids;             //client process ids
int clientCount;
int threadCount;

//syn variables
pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
sem_t empty;
sem_t full;





//method declarations
int getArguments(int argc, char **argv);
void signal_handler(int signal);
void freeResources();
void* handle_client(void* arg);


//queue method declarations
ClientInfo* initQueue(ClientInfo queue[QUEUE_SIZE]);
ClientInfo* addCLient(ClientInfo queue[], ClientInfo client);
ClientInfo removeClient(ClientInfo queue[QUEUE_SIZE]);
void printQueue(ClientInfo queue[QUEUE_SIZE]);



//QUEUE FUNCTIONALITY--------------------------------------------------------------------------------
ClientInfo* initQueue(ClientInfo queue[QUEUE_SIZE]) {
    int i;
    currentSize=0;
    for (i = 0; i < QUEUE_SIZE; i++) {
        strcpy(queue[i].dirPath, DEFAULT_VALUE);
        strcpy(queue[i].clientIP, DEFAULT_VALUE);
        queue[i].socketFd = -1;
    }
}

ClientInfo* addCLient(ClientInfo queue[], ClientInfo client){
    int i;
    for (i = 0; i < QUEUE_SIZE; i++) {
        //latest added client has found
        if(strcmp(queue[i].dirPath,DEFAULT_VALUE) == 0){
            //printf("\n\n%d Adding client %d %s %s\n\n", i, client.clientPort, client.dirPath, client.clientIP);
            queue[i] = client;
            currentSize += 1;
            return queue;
        }
    }
}

ClientInfo removeClient(ClientInfo queue[QUEUE_SIZE]) {
    int i;
    ClientInfo client = queue[0]; // store the first element in a temporary variable

    for (i = 0; i < QUEUE_SIZE-1; i++) {
        //shift each element one position to the left
        queue[i] = queue[i+1]; 
    }

    currentSize -= 1;
    return client; 
}

void printQueue(ClientInfo queue[QUEUE_SIZE]) {
    int i;
    printf("\n--QUEUE--\n");
    printf("Queue's element size: %d.\n", currentSize);
    for (i = 0; i < QUEUE_SIZE; i++) {
        if(strcmp(queue[i].dirPath,DEFAULT_VALUE) != 0){
            printf("%d - path:%s client IP:%s clietn socket fd:%d \n", i, queue[i].dirPath, queue[i].clientIP, queue[i].socketFd);
            return;
        }
    }

    printf("-------\n\n");
}