//GONCA EZGI CAKIR 
//151044054
//CSE344 MIDTERM PROJECT - biboServer Header


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <dirent.h>


#define CHUNK_SIZE 1024  // adjust as necessary
#define MAX_REQUEST_LENGTH 1024
#define MAX_REQUEST_TOKEN_LENGTH 128
#define MAX_COMMAND_BUFFER 2048
#define MAX_READ_COMMAND_BUFFER 4096
#define MAX_TOKEN_LENGTH 256
#define CLIENT_SIZE 1024
#define LINE_SIZE 2048
#define LOGFILE_NAME "logfile"
#define TEMP_FILE_PATH "ezgiCakirServerTemp"  // Define your temporary file path


struct BiboClient{
    char serverPid[30];
    char connectInfo[30];   //ty connoct/connect
    char clientPid[30];
    char clientId[30];  //client01,client02..etc
};



//shared memory struct
struct SharedMemory{

    int clientCount;  
    //counting semaphore for max client      
    sem_t sem_clients;
    //log file semaphore
    sem_t sem_logfile;
    //client queue semaphore 
    sem_t sem_queue;
    int workingChildren;

    //reader/writer semaphores----
    sem_t readTry;
    sem_t rmutex;
    sem_t rsc;
    sem_t wmutex;
    //reader counter
    int readCount;
    //writer counter
    int writeCount;

};


//bibo server information struct
struct BiboServer{

    char dirName[20]; 
    int maxClient;   
};


//method declarations
int getArguments(int argc, char **argv);
int checkValidity();
void signal_handler(int signal);
void signal_handler_child(int signal);
struct BiboClient readServerFifo();
void readClientRequest();
char* help_command(int key, char* command);
char* quit_command();
char* list_command();
char* load_command(char* source, char* destination);
char* read_command(char* filename, int num_line);
char* findClientId(char* clientPid);
void freeResources();



struct BiboClient* initQueue(struct BiboClient array[CLIENT_SIZE]) {
    int i;
    for (i = 0; i < CLIENT_SIZE; i++) {
        strcpy(array[i].serverPid, "X");
        strcpy(array[i].connectInfo, "X");
        strcpy(array[i].clientPid, "X");
        strcpy(array[i].clientId, "X");

    }
}

struct BiboClient* addCLient(struct BiboClient array[], struct BiboClient client){
    int idx;
    for (idx = 0; idx < CLIENT_SIZE; idx++) {
        if(strcmp(array[idx].serverPid,"X") == 0){
            array[idx] = client;
            return array;
        }
    }
}

struct BiboClient removeClient(struct BiboClient clientQueue[CLIENT_SIZE]) {
    int i;
    struct BiboClient item = clientQueue[0]; // Store the first element in a temporary variable

    for (i = 0; i < CLIENT_SIZE-1; i++) {
        clientQueue[i] = clientQueue[i+1]; // Shift each element one position to the left
    }

    return item; // Place the first element at the end of the array
}

void printArray(struct BiboClient array[CLIENT_SIZE]) {
    int i;
    for (i = 0; i < CLIENT_SIZE; i++) {
        if(strcmp(array[i].serverPid,"X") == 0){
            return;
        }
    }
}