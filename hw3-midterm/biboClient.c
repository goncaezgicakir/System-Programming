//GONCA EZGI CAKIR 
//151044054
//CSE344 MIDTERM PROJECT - biboClient

#include "biboClient.h"

//global variables
static volatile sig_atomic_t keepRunning;
struct BiboClient biboClient;



int main(int argc, char **argv){

    //set while loop control variable
    keepRunning=1;

    //signal handling----------------------------------------------------------------------------------
    struct sigaction act;
    memset(&act, 0, sizeof(act));
    act.sa_handler = signal_handler;
    act.sa_flags = 0;
    sigaction(SIGINT, &act, NULL);

    //get console arguments ----------------------------------------------------------------------------
    //get client process pid 
    int pid = getpid();
    if(getArguments(argc, argv, pid) == -1){
        return -1;
    }

    //send info to server through fifo-----------------------------------------------------------------
    connectServerFifo();

    //get request and send to server--------------------------------------------------------------------
    umask(0);
    int errno;
    if ((errno = mkfifo(biboClient.clientPid, S_IRUSR | S_IWUSR | S_IWGRP) == 1) && (errno != EEXIST)){
        perror("Client fifo is already exist.");
        exit(EXIT_FAILURE);
    }
    
    //runs till "quit" command entered----------------------------------------------------------------
    while(keepRunning){

        //quit command entered
        sendRequest();
    }   
    
    return 0;
}



//function to send client info to server
void connectServerFifo(){
    
    int server_fd;

    //open server fifo
    if ((server_fd = open(biboClient.serverPid, O_WRONLY , 0666)) == -1){
        perror("Failed to open server fifo.");
        exit(EXIT_FAILURE);
    }

    //write to server fifo
    if(write(server_fd, &biboClient, sizeof(biboClient)) == -1){
        perror("Failed to write server fifo.");
        exit(EXIT_FAILURE);
    }

    //close server fifo
    if(close(server_fd)== -1){
        perror("Failed to close server fifo.");
        exit(EXIT_FAILURE);
    }
}


//function to send client request to server
void sendRequest(){
    
    int client_fd;

    //SEND REQUEST----------------------------------------------------------
    //open client fifo
    if ((client_fd = open(biboClient.clientPid, O_WRONLY | O_CREAT)) == -1){
        perror("Failed to open client fifo.");
        exit(EXIT_FAILURE);
    }

    //get client request from user
    char clientRequest[MAX_REQUEST_LENGTH];
    memset(clientRequest, 0, sizeof(clientRequest));
    printf(">>Enter comment:");
    fflush(stdin);
    fgets(clientRequest, MAX_REQUEST_LENGTH, stdin);
    clientRequest[strlen(clientRequest)-1] = '\0';

    //if the quit entered client terminal needs to see this message
    if(strcmp(clientRequest,"quit") == 0 ){
        printf("\nSending write request to server log file\nwaiting for logfile.");
    }

    //if the quit entered client terminal needs to see this message
    if(strcmp(clientRequest,"killServer") == 0 ){
        signal_handler(SIGINT);
    }

    //write request to client fifo
    if (write(client_fd, &clientRequest, MAX_REQUEST_LENGTH) == -1){
        perror("Failed to write client fifo.");
        exit(EXIT_FAILURE);
    }

    //close client fifo
    if (close(client_fd)){
        perror("Failed to close client fifo.");
        exit(EXIT_FAILURE);
    }
    
    //GET RESPONSE---------------------------------------------------------   
    //---------------------------------------------------------------------
    //1.get response size first
    if ((client_fd = open(biboClient.clientPid, O_RDONLY | O_CREAT)) == -1){
        perror("Failed to open client fifo.");
        exit(EXIT_FAILURE);
    }

    //read response size from client fifo
    size_t responseSize;
    memset(&responseSize, 0, sizeof(size_t));
    if(read(client_fd, &responseSize, sizeof(size_t)) == -1){
        perror("Failed to read from  client fifo - response size.");
        exit(EXIT_FAILURE);
    }
   
    
    //---------------------------------------------------------------------
    //2.read response from client fifo as size of response
    
    //allocate space for response 
    char* response = calloc(responseSize+1, sizeof(char));

    //read response from client fifo
    if(read(client_fd, response, responseSize) == -1){
        perror("Failed to read from  client fifo - response.");
        exit(EXIT_FAILURE);
    }

    //close client fifo
    if(close(client_fd) == -1){
        perror("Failed to close client fifo- response.");
        exit(EXIT_FAILURE);
    }

    //printf the response for the client side
    printf("\n%s\n\n", response);

    //when quit request entered client closes
    if(strcmp(response,"logfile write request granted.\nbye.\n") == 0 ){
        free(response);
        freeResources();
        keepRunning=0;
        return;
    }
    
    free(response);        

}

//signal handler for SIGINT
//when a SIGINT received writes to server fifo to close all processes
void signal_handler(int signal) {
    if(signal == SIGINT){

        //set the connection info to kill if the client process received SIGINT
        strcpy(biboClient.connectInfo, "");
        strcpy(biboClient.connectInfo, "kill");
        printf("%s", biboClient.connectInfo);

        //write to server fifo to close all clients and server processes-
        int server_fd;
        //open server fifo
        if ((server_fd = open(biboClient.serverPid, O_WRONLY | O_CREAT)) == -1){
            perror("Failed to open server fifo.");
            exit(EXIT_FAILURE);
        }

        //write to server fifo
        if(write(server_fd, &biboClient, sizeof(biboClient)) == -1){
            perror("Failed to write server fifo.");
            exit(EXIT_FAILURE);
        }

        //close server fifo
        if(close(server_fd)== -1){
            perror("Failed to close server fifo.");
            exit(EXIT_FAILURE);
        }

        printf("\nSIGINT signal is catched.\n");
        
    }

    if (signal == SIGUSR1){
        printf("\nTerminated.\n");
    }

    keepRunning=0;
    freeResources();
    exit(EXIT_SUCCESS);
    
}


//function to get arguments
int getArguments(int argc, char **argv, int pid){
    
    //check that is there enough console arguments
    if(argc != 3){
        perror("Usage: There shold be 3 console arguments. [biboClient <Connect/tryConnect> ServerPID]");
        return 1;
    }
    strcpy(biboClient.connectInfo, "");
    strcpy(biboClient.serverPid, "");
    strcpy(biboClient.clientPid, "");
    strcpy(biboClient.clientId, "");

    //set client pid and id info to struct
    char clientPid[30];
    sprintf(clientPid, "%d", pid);
    strcat(biboClient.clientPid, clientPid);
    biboClient.clientPid[strlen(biboClient.clientPid)] = '\0';

    strcpy(biboClient.clientId, "client");

    //store connection info and server pid
    strcat(biboClient.connectInfo, argv[1]);
    biboClient.connectInfo[strlen(biboClient.connectInfo)] = '\0';
    strcat(biboClient.serverPid, argv[2]);
    biboClient.serverPid[strlen(biboClient.serverPid)] = '\0';

    return 0;
}


//free all allocated spaces
void freeResources(){
    
    unlink(biboClient.clientPid);
}







