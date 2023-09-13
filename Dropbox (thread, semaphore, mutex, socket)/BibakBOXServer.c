//GONCA EZGI CAKIR 
//151044054
//CSE344 FINAL PROJECT - BibakBOXServer

#include "BibakBOXServer.h"

int removeControl;

int main(int argc, char **argv){

    //initalize global counters
    clientCount = 0;
    threadCount = 0;

    //while loop control variable set
    keepRunning = 1;
    
    //controller for removing files
    removeControl = 0;

    //signal handling---------------------------------------------------------------------------------
    struct sigaction act;
    memset(&act, 0, sizeof(act));
    act.sa_handler = signal_handler;
    act.sa_flags = 0;
    sigaction(SIGINT, &act, NULL);   
    
    printf("SERVER SIDE\n");

    //get arguments fails-----------------------------------------------------------------------------
    if(getArguments(argc, argv) == -1){
        exit(EXIT_FAILURE);
    }   
    
    //initialize semaphore----------------------------------------------------------------------------
    if (sem_init(&empty, 1, poolSize) == -1){
        perror("Failed to create empty semaphore.\n");
        exit(EXIT_FAILURE);
    }

    if (sem_init(&full, 1, 0) == -1){
        perror("Failed to create full semaphore.\n");
        exit(EXIT_FAILURE);
    }
    //create and initialize client queue--------------------------------------------------------------
    clientQueue = calloc(QUEUE_SIZE, sizeof(ClientInfo));
    initQueue(clientQueue);
    //printQueue(clientQueue);
    

    //CREATE SERVER SOCKET------------------------------------------------------------------------------
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) {
        perror("Error creating server socket.");
        exit(EXIT_FAILURE);
    }

    //set server address
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    serverAddress.sin_port = htons(serverPort);

    //bind the server socket to the specified address and port
    if (bind(serverSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0) {
        perror("Binding server socket.");
        exit(EXIT_FAILURE);
    }

    //listen for client connections
    if (listen(serverSocket, QUEUE_SIZE) < 0) {
        perror("Listening on server socket for connections.");
        exit(EXIT_FAILURE);
    } 
    
    //allocate space for client process pids
    client_pids = (pid_t *)malloc(QUEUE_SIZE * sizeof(pid_t));

    //creating thread pool ---------------------------------------------------------------------------
    printf("Thread pool is creating with size %d.\n", poolSize);
    tids = (pthread_t *)malloc(poolSize * sizeof(pthread_t));

   //create threads
    for (int i = 0; i < poolSize; i++) {
        int* t_id = (int*)malloc(sizeof(int));
        *t_id = i;
        pthread_create(&tids[i], NULL, handle_client, t_id);
    }


    //SERVER STARTS------------------------------------------------------------------------------
    printf("Server is waiting for clients.....\n\n");

    //accept client connections and handle them in separate threads
    while (keepRunning) {

        socklen_t serverAddressLength = sizeof(serverAddress);
        int socketFd_s = accept(serverSocket, (struct sockaddr *)&serverAddress, &serverAddressLength);

        if (socketFd_s < 0) {
            perror("Server socket accepting failed.");
            continue;
        }

        //receive client information
        ClientInfo client;
        //add client socket fd to its struct
        if (recv(socketFd_s, &client, sizeof(ClientInfo), 0) == -1) {
            perror("Receiving client information failed.");
            close(socketFd_s);
            continue;
        }
        client.socketFd = socketFd_s;

        //all threads are full, refus the client
        if(threadCount == poolSize){
            printf("Client %d (PID) is refused, all threads are occupied.\n", client.pid);
            kill(client.pid, SIGKILL);
            continue;
        }
        

        //get client info through socket
        sem_wait(&empty);
        pthread_mutex_lock(&queue_mutex);
        addCLient(clientQueue, client);
        clientCount += 1;
        printf("Client %d (PID) request received.\n", client.pid);

        client_pids[clientCount-1] = client.pid;
        //printQueue(clientQueue);
        pthread_mutex_unlock(&queue_mutex);
        sem_post(&full);

    }

    //close the server socket
    freeResources();

    return 0;
}




//-------------------------------------------------------------------------------------------------------------------------------------------------
//thread to handle client request
void* handle_client(void* arg) {
    
    int* t_id = (int*)arg;
    int id = *t_id;
    free(arg);

    printf("Thread%d is created.\n", id);
    

    //decrease semaphore before executing child
    sem_wait(&full);

    pthread_mutex_lock(&queue_mutex);
    ClientInfo client = removeClient(clientQueue);
    threadCount += 1;
    pthread_mutex_unlock(&queue_mutex);

    removeControl = 0;
    
    printf("Thread%d is handling client %d (PID).\n", id, client.pid);

    while(keepRunning){


        //SEND CONTENT TO CLIENT----------------------------------------------------------
        //send server content to client
        //traverse server current path adn store them in a array of fileInfo struct
        FileInfo* serverFileContent = calloc(MAX_FILE, sizeof(FileInfo));
        int serverContentSize = 0;
        initFileContentArray(serverFileContent);
        serverFileContent = traverseDirectory(directory, serverFileContent, &serverContentSize, 1);
        
        for(int i=0; i<serverContentSize; i++){
            //send file information to current client
            if (send(client.socketFd, &(serverFileContent[i]), sizeof(FileInfo), 0) == -1) {
                perror("Sending FileInfo to client failed. (thread)");
                exit(EXIT_FAILURE);
            }
        }

        //send the end notify(all) to server
        FileInfo endNotify_A;
        endNotify_A.type = 3;
        if (send(client.socketFd, &endNotify_A, sizeof(FileInfo), 0) == -1) {
            perror("Sending endNotify(all) struct to client failed. (thread)");
            exit(EXIT_FAILURE);
        }

        //printf("\n\nSERVER CONTENT\n------------------\n");
        //printFileContentArray(serverFileContent);


        //GET CLIENT DIFFERENCE CONTENT------------------------------------------------------------------------------------
        //receive client difference file content from client
        //allocate space for the client difference content 

        FileInfo* clientFileDifferenceContent = calloc(MAX_FILE, sizeof(FileInfo));
        initFileContentArray(clientFileDifferenceContent);
        FileInfo temp_D;

        for(int i=0; i<MAX_FILE; i++){
            
            //receive server file content
            if (recv(client.socketFd, &temp_D, sizeof(FileInfo), 0) == -1) {
                perror("Receiving server file content failed.");
                close(client.socketFd);
                exit(EXIT_FAILURE);
            }

            //end notify received break the loop
            if(temp_D.type == 3){
                break;
            }

            //store the file content
            clientFileDifferenceContent[i] = temp_D;
        }

        //printf("\n\nCLIENT DIFFERENCE CONTENT\n------------------\n");
        //printFileContentArray(clientFileDifferenceContent);    //current client's file content array
        

        //SEND MISSING FILE CONTENT TO CLIENT-------------------------------------------------------------------------
        int clientContentDifferenceSize = getFileContentSize(clientFileDifferenceContent);
        
        for(int i=0; i<clientContentDifferenceSize; i++){

            //if the file type is 1 (FILE)
            if(clientFileDifferenceContent[i].type == 1){
                char* filename = (char*)malloc(256 * sizeof(char));        
                strcpy(filename, "");
            
                sprintf(filename,"%s/%s", serverMainDir, clientFileDifferenceContent[i].path);
                
                FILE* file = fopen(filename, "r");
                if (file == NULL) {
                    perror("Error opening file");
                    exit(EXIT_FAILURE);
                }

                char ch;
                for (int j=0; j<clientFileDifferenceContent[i].bytesRead; j++) {
                    ch = fgetc(file);
                    send(client.socketFd, &ch, sizeof(char), 0);  // Send each character through the socket
                }

                // Close the file and socket
                fclose(file);   
                free(filename);
            } 
        }

        free(clientFileDifferenceContent);

        //GET CLIENT CONTENT----------------------------------------------------------------------

        //allocate space for the client content 
        FileInfo* clientFileContent = calloc(MAX_FILE, sizeof(FileInfo));
        initFileContentArray(clientFileContent);

        FileInfo temp_A;
        for(int i=0; i<MAX_FILE; i++){

            //receive server file content
            if (recv(client.socketFd, &temp_A, sizeof(FileInfo), 0) == -1) {
                perror("Receiving client file content failed.");
                close(client.socketFd);
                exit(EXIT_FAILURE);
            }

            //end notify received break the loop
            if(temp_A.type == 3){
                break;
            }

            //store the file content
            clientFileContent[i] = temp_A;
        }

        //printf("\n\nCLIENT CONTENT\n--------------------\n");
        //printFileContentArray(clientFileContent);
        
        
        //SEND DIFFERENCES TO CLIENT---------------------------------------------------------------------------------------
        //get differences from client content then our content
        //send these difference file content to client
        int currentClientContentSize = getFileContentSize(clientFileContent);
        FileInfo* fileContentDifferences = calloc(MAX_FILE, sizeof(FileInfo));
        initFileContentArray(fileContentDifferences);
        
        /*if(removeControl == 1){
            FileInfo* removeDifferences = calloc(MAX_FILE, sizeof(FileInfo));
            initFileContentArray(removeDifferences);
            removeDifferences = findDifferences(serverFileContent, clientFileContent, serverContentSize, currentClientContentSize);
            int removeDifferencesSize = getFileContentSize(removeDifferences);
            
            for(int i=0; i<removeDifferencesSize; i++){
                remove(removeDifferences[i].path);
                printf("---\n\n REMOVED %s -----\n\n", removeDifferences[i].name);
            }

            free(removeDifferences);

        }

        //update server file content
        free(serverFileContent);
        serverFileContent = calloc(MAX_FILE, sizeof(FileInfo));
        serverContentSize = 0;
        initFileContentArray(serverFileContent);
        serverFileContent = traverseDirectory(directory, serverFileContent, &serverContentSize, 1);*/


        //store differences
        fileContentDifferences = findDifferences(clientFileContent, serverFileContent, currentClientContentSize, serverContentSize);
        
        //printf("\n\nMY DIFFERENCE CONTENT\n------------------\n");
        //printFileContentArray(fileContentDifferences);
        
        int differenceContentSize = getFileContentSize(fileContentDifferences);
     
        //send difference content file array to server to notify the missing files
        for(int i=0; i<differenceContentSize; i++){
            
            //send file information to current client
            if (send(client.socketFd, &(fileContentDifferences[i]), sizeof(FileInfo), 0) == -1) {
                perror("Sending difference FileInfo to server failed.");
                exit(EXIT_FAILURE);
            }
        }

        //send the end notify (difference) to server
        FileInfo endNotify_D;
        endNotify_D.type = 3;
        if (send(client.socketFd, &endNotify_D, sizeof(FileInfo), 0) == -1) {
            perror("Sending endNotify(difference) struct to client failed.");
            exit(EXIT_FAILURE);
        }
        

        //GET MISSING FILE CONTENT FROM CLIENT-------------------------------------------------------------------------

        for(int i=0; i<differenceContentSize; i++){

            char* filename = (char*)malloc(256 * sizeof(char));         strcpy(filename, "");
            sprintf(filename,"%s/%s", serverMainDir, fileContentDifferences[i].path);
            //if the file type is 1 (FILE)
            
            if(fileContentDifferences[i].type == 1){
                FILE* file = fopen(filename, "wb+");
                if (file == NULL) {
                    perror("Error opening file");
                    exit(EXIT_FAILURE);
                }

                char ch;
                for (int j=0; j<fileContentDifferences[i].bytesRead; j++) {
                    recv(client.socketFd, &ch, sizeof(char), 0);
                    fputc(ch, file);  // Write each received character into the file
                }
                fclose(file);
            }

            else if(fileContentDifferences[i].type == 2){
                mkdir(filename, 0777);
            }

            free(filename);

        }

        //free file content arrays
        free(serverFileContent);
        free(clientFileContent);
        free(fileContentDifferences);
        //--------------------------------------------------------------------------------------------------
        removeControl = 1;
        //sleep(1);
    }
    
    sem_post(&empty);

    printf("Thread%d is finished.\n", id);
    return (void*)0;
}



//-------------------------------------------------------------------------------------------------------------------------------------------------
//signal handler for server 
void signal_handler(int signal) {
    if(signal == SIGINT){
        printf("\n------SIGINT signal is catched-------\n");
        keepRunning = 0;

        //kill clients
        for(int i=0; i<clientCount; i++){
            kill(client_pids[i], SIGINT);
        }

        //free all resources
        freeResources();
        exit(EXIT_SUCCESS);
    }
   
}


///function to get arguments
int getArguments(int argc, char **argv){
    
    //check that is there enough console arguments
    if(argc != 4){
        perror("Usage: There should be 4 console arguments. [ BibakBOXServer <directory> <threadPoolSize> <portnumber>");
        return -1;
    }
    

    //store arguments
    strcpy(directory, "");
    strcpy(directory, argv[1]);
    poolSize = atoi(argv[2]);
    serverPort = atoi(argv[3]);

    strcpy(serverMainDir, getFileName(directory));
    //printf("dir:%s  poolsize:%d  server port:%d\n", directory, poolSize, serverPort);

    return 0;
}



//frees all allocated resources 
void freeResources(){

    for(int i=0; i<poolSize; i++){
        pthread_join(tids[i], NULL);
    }

    close(serverSocket);
    free(clientQueue);
    free(client_pids);
    free(tids);

    sem_destroy(&empty);
    sem_destroy(&full);
    pthread_mutex_destroy(&queue_mutex);      

}


