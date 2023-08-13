//CSE344 MIDTERM
//GONCA EZGI CAKIR 
//151044054
//Program for biboServer
#include "biboServer.h"

static volatile sig_atomic_t keepRunning;
struct BiboServer biboServer;   //struct for server variables

FILE* log_fd;   //log file fd
int server_fd;  //server fifo fd
DIR *d; //server's directory
FILE* tempFile; //server double int checker fd

int shm_fd; // shared memory fd
struct SharedMemory *sharedMemory; //a shared memory
int queue_fd;   //client queue shared memory fd
struct BiboClient* clientQueue; //client request queue (as a shared memory)

pthread_mutex_t empty_queue = PTHREAD_MUTEX_INITIALIZER;

//TODO: dinamik yapılabilir
pthread_t threadPids[CLIENT_SIZE];
int clientPids[CLIENT_SIZE];    //client pid array
char serverPid[30];    //server pid

//for client's child
struct sigaction child_act;


int main(int argc, char **argv){

    //while loop control avriable set
    keepRunning = 1;
    
    //signal handling---------------------------------------------------------------------------
    //for server
    struct sigaction act;
    memset(&act, 0, sizeof(act));
    act.sa_handler = signal_handler;
    act.sa_flags = 0;
    sigaction(SIGINT, &act, NULL);   
    

    //get arguments fails-----------------------------------------------------------------------
    if(getArguments(argc, argv) == -1){
        return -1;
    }

    //check double instation of the server process----------------------------------------------    
    tempFile = fopen(TEMP_FILE_PATH, "r");
    if (tempFile != NULL) {
        printf("Another instance of the server process is already running.\n");
        fclose(tempFile);
        exit(1);
    }

    // Create the temporary file
    tempFile = fopen(TEMP_FILE_PATH, "w");
    if (tempFile == NULL) {
        printf("Failed to create the temporary file.\n");
        exit(1);
    }

    //open/create directory --------------------------------------------------------------------
    struct stat st;
    char path[20]="./";
    strcat(path, biboServer.dirName);
    if (stat(path, &st) == -1) {
        // create the directory if it doesn't exist
        if (mkdir(biboServer.dirName, 0777) == -1) {
            perror("mkdir error");
            exit(EXIT_FAILURE);
        }
    }

    // open the directory
    d = opendir(path);
    if (d == NULL) {
        perror("opendir");
        exit(1);
    }

    //create log file  and open it for logging------------------------------------------------
    log_fd = fopen (LOGFILE_NAME, "w+");
    
    //create shared memory space--------------------------------------------------------------
    shm_fd = shm_open("sharedMem", O_CREAT | O_RDWR, 0666); 
    if(shm_fd == -1){
        perror("Failed open shared memory.\n");
        exit(EXIT_FAILURE);
    }
    ftruncate(shm_fd, sizeof(struct SharedMemory));
    sharedMemory = mmap(NULL, sizeof(struct SharedMemory), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    

    //create shared memory space for client queue-----------------------------------------------
    int queue_fd = shm_open("queueMem", O_CREAT | O_RDWR, 0666);
    if(queue_fd == -1) {
        perror("Failed open shared memory.\n");
        exit(EXIT_FAILURE);
    }
    ftruncate(queue_fd, sizeof(struct BiboClient) * 1024);
    clientQueue = (struct BiboClient *) mmap(NULL, sizeof(struct BiboClient) * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, queue_fd, 0);
    if(clientQueue == MAP_FAILED) {
        perror("Failed to map shared memory.\n");
        exit(EXIT_FAILURE);
    }

    //initialize client queue-------------------------------------------------------------------
    initQueue(clientQueue);
    
    //initialize client counter and working children counter to 0-------------------------------
    sharedMemory->clientCount = 0;
    sharedMemory->workingChildren = 0;

    //initialize semaphores---------------------------------------------------------------------
    //counting semaphore to restrict maximum number of children processes to work
    if (sem_init(&(sharedMemory->sem_clients), 1, biboServer.maxClient) == -1){
        perror("Failed to create max clients semaphore.\n");
        exit(EXIT_FAILURE);
    }

    //semaphore to sync queue operations between children processes
    if (sem_init(&(sharedMemory->sem_queue), 1, 1) == -1){
        perror("Failed to create queue semaphore.\n");
        exit(EXIT_FAILURE);
    }

    //semaphore to lock logfile between manupilations
    if (sem_init(&(sharedMemory->sem_logfile), 1, 1) == -1){
        perror("Failed to create logfile semaphore.\n");
        exit(EXIT_FAILURE);
    }

    //reader writer paradigm semaphores and variables
    //initalize reader and writer counters
    sharedMemory->readCount = 0;
    sharedMemory->writeCount = 0;

    //initialize semaphores
    if (sem_init(&(sharedMemory->readTry), 1, 1) == -1){
        perror("Failed to create readTry semaphore.\n");
        exit(EXIT_FAILURE);
    }

    if (sem_init(&(sharedMemory->rmutex), 1, 1) == -1){
        perror("Failed to create rmutex semaphore.\n");
        exit(EXIT_FAILURE);
    }

    if (sem_init(&(sharedMemory->rsc), 1, 1) == -1){
        perror("Failed to create rsc semaphore.\n");
        exit(EXIT_FAILURE);
    }

    if (sem_init(&(sharedMemory->wmutex), 1, 1) == -1){
        perror("Failed to create wmutex semaphore.\n");
        exit(EXIT_FAILURE);
    }


    //SERVER STARTS------------------------------------------------------------------------------
    sprintf(serverPid, "%d", getpid());
    serverPid[strlen(serverPid)] = '\0';
    printf("Server Started PID %d\n", getpid());
    
    //open server fifo --------------------------------------------------------------------------
    umask(0);
    int errno;
    if ((errno = mkfifo(serverPid, S_IRUSR | S_IWUSR | S_IWGRP) == 1) && (errno != EEXIST)){
        perror("Server fifo is already exist.");
        return -1;
    }

    //CREATING THREADS ---------------------------------------------------------------------

    ///lock the empty queue mutex
    pthread_mutex_lock(&empty_queue);
    
    //create the thread pool
   
    for (int i = 0; i < biboServer.poolSize; i++) {
        int* t_id = (int *)malloc(sizeof(int));
        *t_id = i;
        pthread_create(&threadPids[i], NULL, handle_request, t_id);
    }
    printf("Thread pool created with size %d.\n",biboServer.poolSize);


    
    printf("Waiting for clients...\n");

    //loop to keep connecting clients------------------------------------------------------------------------
    while(keepRunning){

        //SERVER PROCESS---------------------------------------------------------------------
        struct BiboClient biboClient = readServerFifo();
        
        //check that client can connect---------------------------------------------------------------------
        //client doesnt wait, this client is terminated
        if(strcmp(biboClient.connectInfo, "tryConnect") == 0 && sharedMemory->workingChildren >0
                                                                 && (sharedMemory->workingChildren == biboServer.maxClient)){
            //add the client to request queue
            printf("Connection request PID %s by tryConnect: Que FULL, can't connect.\n", biboClient.clientPid);               
            unlink(biboClient.clientPid);
            kill(atoi(biboClient.clientPid), SIGUSR1);
            continue;
        }
        //wrong command 
        else if(strcmp(biboClient.connectInfo, "tryConnect") != 0 && strcmp(biboClient.connectInfo, "Connect") != 0){
            printf("Please only use 'Connect' and 'tryConnect' commands. Be aware of case sensitivity.\n");
            unlink(biboClient.clientPid);
            kill(atoi(biboClient.clientPid), SIGUSR1);
            continue;
        }

        //this section means client can connect------------------------------------------------------------------        
        //add the client to request queue
        sem_wait(&(sharedMemory->sem_queue));
        sharedMemory->clientCount = sharedMemory->clientCount + 1;
       
        //create clientID according to client counter which is in the shared memory section
        char counter[3];
        sprintf(counter, "%d", sharedMemory->clientCount);
        if(sharedMemory->clientCount < 10){
            strcat(biboClient.clientId ,"0");
        }
        strcat(biboClient.clientId ,counter);

        addCLient(clientQueue, biboClient);
        //increment client counter in the shared memory
        
        //unlock the empty queue mutex
        pthread_mutex_unlock(&empty_queue);

        printArray(clientQueue);
        int idx = sharedMemory->clientCount-1;
        //store client pid
        clientPids[idx] = atoi(biboClient.clientPid);
        sem_post(&(sharedMemory->sem_queue));
       

        //max client check (client waits to connect)
        if(sharedMemory->workingChildren == biboServer.maxClient){
            printf("Connection request PID %s Que FULL\n", biboClient.clientPid);
        }

      
        //-------------------------------------------------------------------------

    }
    
    return 0;
}



//thread function in order to handle the request
void* handle_request(void* arg) {

    child_act.sa_handler = signal_handler_child;
    child_act.sa_flags = 0; 
    sigemptyset(&child_act.sa_mask);
    sigaction(SIGINT, &child_act, NULL); 

    while(1){
        //if queue is empty then lock the mutex
        if(curSize == 0){
            pthread_mutex_lock(&empty_queue);
        }

     
        //decrease semaphore before executing child
        sem_wait(&(sharedMemory->sem_clients));
        
        //increment working child process at the time
        sharedMemory->workingChildren = sharedMemory->workingChildren + 1;

    
        //decrease queue semaphore before pop client request
        sem_wait(&(sharedMemory->sem_queue));
        struct BiboClient biboClient = removeClient(clientQueue);

       
        printf("Client PID %s connected as “%s”\n", biboClient.clientPid, biboClient.clientId);
        //increase queue semaphore after pop client request
        sem_post(&(sharedMemory->sem_queue));


        //loop to keep getting request from a releated client    
        while(1){

            //open client fifo
            int client_fd;
            if ((client_fd = open(biboClient.clientPid, O_RDONLY | O_CREAT)) == -1){
                perror("Failed to open client fifo.");  
                exit(EXIT_FAILURE);
            }

            //GET REQUEST--------------------------------------------------------------------------
            //read the request from client fifo
            char requestTempRW[MAX_REQUEST_LENGTH];
            char clientRequest[MAX_REQUEST_LENGTH];
            if(read(client_fd, &clientRequest, MAX_REQUEST_LENGTH)== -1){
                perror("Failed to read client fifo.");  
                exit(EXIT_FAILURE);
            }

            strcpy(requestTempRW,clientRequest);

            //close client fifo
            if(close(client_fd)== -1){
                perror("Failed to close client fifo");  
                exit(EXIT_FAILURE);
            }        

            //parse client request---------------------------------------------------------------------
            
            char* requestTokens[4];
            // Initialize the array with "empty" strings
            for(int i = 0; i < 4; i++) {
                requestTokens[i] = "empty";
            }

            // Split the client request by space
            char* token = strtok(clientRequest, " ");
            int i = 0;
            while(token != NULL && i < 4) {
                requestTokens[i] = token;
                token = strtok(NULL, " ");
                i++;
            }
            
           

            //SEND RESPONSE--------------------------------------------------------------------------
            char* responseTemp;
            size_t responseSize;
            char* response;

            //open client fifo to write
            if ((client_fd = open(biboClient.clientPid, O_WRONLY | O_CREAT)) == -1){
                perror("Failed to open client fifo.");  
                exit(EXIT_FAILURE);
            }

            //REQUEST LIST------------------------------------------------------------------------------------
            //if the request quit, end-----------------------------------------------------------
            if(strcmp(requestTokens[0],"quit") == 0){
                responseTemp = quit_command();
                responseSize = strlen(responseTemp);
                response = calloc(responseSize+1, sizeof(char));
                strcpy(response,responseTemp);

                //----------------------------------------------------------------------------------
                //1.send response size
                //write response to client fifo
                if(write(client_fd, &responseSize, sizeof(size_t)) == -1){
                    perror("Failed to write client fifo - response size.");  
                    exit(EXIT_FAILURE);
                }

                //----------------------------------------------------------------------------------
                //2.send response
                //write response to client fifo
                if(write(client_fd, response, responseSize) == -1){
                    perror("Failed to write client fifo - response.");  
                    exit(EXIT_FAILURE);
                }
                //close client fifo
                if(close(client_fd)== -1){
                    perror("Failed to close client fifo - response.");  
                    exit(EXIT_FAILURE);
                }
                
                sem_wait(&(sharedMemory->sem_logfile));
                fprintf(log_fd, "Client Pid: %s, Client Id: %s quited.\n", biboClient.clientPid, biboClient.clientId);
                printf("%s disconnected.\n", biboClient.clientId);
                sem_post(&(sharedMemory->sem_logfile));
                free(response);
                break;
            }
            //if the request killServer-------------------------------------------------------------
            else if(strcmp(requestTokens[0],"killServer") == 0){
                sem_wait(&(sharedMemory->sem_logfile));
                fprintf(log_fd, "Client Pid: %s, Client Id: %s killed the server.\n", biboClient.clientPid, biboClient.clientId);
                sem_post(&(sharedMemory->sem_logfile));
                unlink(biboClient.clientPid);
                signal_handler(SIGINT);
            }
            //other valid requests----------------------------------------------------------------
            else if(strcmp(requestTokens[0],"list") == 0 || strcmp(requestTokens[0],"help") == 0 ||
                strcmp(requestTokens[0],"upload") == 0 || strcmp(requestTokens[0],"download") == 0 ||
                strcmp(requestTokens[0],"writeT") == 0 || strcmp(requestTokens[0],"readF") == 0){
                
                //-----------------------------------------------------------------------------------
                //get command's response
                //help command entered
                if(strcmp(requestTokens[0],"help") == 0){
                    //list all command (exp: help)
                    if(strcmp(requestTokens[1], "empty") == 0){
                        responseTemp = help_command(1, "");
                    } 
                    //give the detail for requested command (exp: help readF)
                    else {
                        responseTemp = help_command(2, requestTokens[1]);
                    }
                }
                //list command entered
                else if (strcmp(requestTokens[0],"list") == 0){
                    responseTemp = list_command();
                }
                //upload- download command entered
                else if( (strcmp(requestTokens[0],"upload") == 0) || (strcmp(requestTokens[0],"download") == 0) ){

                    char sourcePath[30] = "";
                    char destinationPath[30] = "";
                    if (strcmp(requestTokens[0],"upload") == 0){
                        strcpy(sourcePath, requestTokens[1]);
                        strcpy(destinationPath, biboServer.dirName);
                        strcat(destinationPath, "/");
                        strcat(destinationPath, requestTokens[1]);
                    } 
                    else if (strcmp(requestTokens[0],"download") == 0){
                        strcpy(destinationPath, requestTokens[1]);
                        strcpy(sourcePath, biboServer.dirName);
                        strcat(sourcePath, "/");
                        strcat(sourcePath, requestTokens[1]);
                    }

                    //----------------------------------------------------
                    //WRITER
                    sem_wait(&(sharedMemory->wmutex));
                    sharedMemory->writeCount = sharedMemory->writeCount + 1;

                    if(sharedMemory->writeCount == 1){
                        sem_wait(&(sharedMemory->readTry));
                    }

                    sem_post(&(sharedMemory->wmutex));
                    sem_wait(&(sharedMemory->rsc));
                    //----------------------------------------------------

                    if(strcmp(requestTokens[1], LOGFILE_NAME) == 0){
                        responseTemp = "Client doesn't have permission to acces to logfile.\n";
                    } else {
                        responseTemp = load_command(sourcePath, destinationPath);
                    }

                    //----------------------------------------------------
                    //WRITER
                    sem_post(&(sharedMemory->rsc));
                    sem_wait(&(sharedMemory->wmutex));
                    sharedMemory->writeCount = sharedMemory->writeCount - 1;

                    if(sharedMemory->writeCount == 0){
                        sem_post(&(sharedMemory->readTry));
                    }

                    sem_post(&(sharedMemory->wmutex));
                    //----------------------------------------------------

                }
                //readF command entered
                else if((strcmp(requestTokens[0],"readF") == 0)){
                    
                    //----------------------------------------------------
                    //READER
                    sem_wait(&(sharedMemory->readTry));
                    sem_wait(&(sharedMemory->rmutex));
                    sharedMemory->readCount = sharedMemory->readCount + 1;

                    if(sharedMemory->readCount == 1){
                        sem_wait(&(sharedMemory->rsc));
                    }

                    sem_post(&(sharedMemory->rmutex));
                    sem_post(&(sharedMemory->readTry));

                    //----------------------------------------------------

                    //if there is no number line
                    if(strcmp(requestTokens[2], "empty") == 0){

                        responseTemp = read_command(requestTokens[1], -1);
                    }
                    //when there is a number line 
                    else {

                        responseTemp = read_command(requestTokens[1], atoi(requestTokens[2]));
                    }

                    //----------------------------------------------------
                    //READER
                    sem_wait(&(sharedMemory->rmutex));
                    sharedMemory->readCount = sharedMemory->readCount - 1;

                    if(sharedMemory->readCount == 0){
                        sem_post(&(sharedMemory->rsc));
                    }

                    sem_post(&(sharedMemory->rmutex));
                    //----------------------------------------------------
                }
                //writeT command entered
                else if((strcmp(requestTokens[0],"writeT") == 0)){
                    
                    char command[10];
                    char filename[50];
                    int lineNumber;
                    char line[100];
                    //if no line is given
                    //write at the end of the file
                    if(atoi(requestTokens[2]) == 0){
                        sscanf(requestTempRW, "%s %s %[^\n]", command, filename, line);
                        response = write_command(filename, line, -1);
                    }
                    
                    //write to given number of the line
                    else{
                        int result = sscanf(requestTempRW, "%s %s %d %[^\n]", command, filename, &lineNumber, line);
                        responseTemp = write_command(filename, line, lineNumber);
                    }
                }

                //--------------------------------------------------------------------------------------------
                //1.send response size

                responseSize = strlen(responseTemp);
                response = calloc(responseSize+1, sizeof(char));
                strcpy(response,responseTemp);

                //allocated space freed only when list/readF hole file command received
                if((strcmp(requestTokens[0],"list") == 0) || 
                    ((strcmp(requestTokens[0],"readF") == 0) && (strcmp(requestTokens[2],"empty") == 0))){
                    
                    free(responseTemp);
                }

                //write response to client fifo
                if(write(client_fd, &responseSize, sizeof(responseSize)) == -1){
                    perror("Failed to write client fifo - response size.");  
                    exit(EXIT_FAILURE);
                }

                //----------------------------------------------------------------------------------
                //2.send response
            
                //write response to client fifo
                if(write(client_fd, response, responseSize) == -1){
                    perror("Failed to write client fifo - response.");  
                    exit(EXIT_FAILURE);
                }
                //close client fifo
                if(close(client_fd)== -1){
                    perror("Failed to close client fifo - response.");  
                    exit(EXIT_FAILURE);
                }
                //allocated space freed 
                free(response);

            }
            //İnvalid commands
            else {
                char errorTemp[MAX_REQUEST_LENGTH];
                sprintf(errorTemp, "Command '%s' is not supported.", requestTokens[0]);
                responseSize = strlen(errorTemp);
                response = calloc(responseSize+1, sizeof(char));
                strcpy(response,errorTemp);

                //----------------------------------------------------------------------------------
                //1.send response size
                //write response size to client fifo
                if(write(client_fd, &responseSize, sizeof(responseSize)) == -1){
                    perror("Failed to write client fifo - response size.");  
                    exit(EXIT_FAILURE);
                }

                //----------------------------------------------------------------------------------
                //2.send response
                //write response to client fifo
                if(write(client_fd, response, responseSize) == -1){
                    perror("Failed to write client fifo - response.");  
                    exit(EXIT_FAILURE);
                }
                //close client fifo
                if(close(client_fd)== -1){
                    perror("Failed to close client fifo - response.");  
                    exit(EXIT_FAILURE);
                }
                free(response);
            }

            //write reuest into log file
            sem_wait(&(sharedMemory->sem_logfile));
            fprintf(log_fd, "Client Pid: %s, Client Id: %s, Request: %s\n", biboClient.clientPid, biboClient.clientId, clientRequest);
            sem_post(&(sharedMemory->sem_logfile));
        }
       

        //increase semaphore after executing child
        sem_post(&(sharedMemory->sem_clients));
        
        //decrease working child process at the time
        sharedMemory->workingChildren = sharedMemory->workingChildren - 1;
    }
    return (void*)0;
}



//reads data from fifo for client informations
struct BiboClient readServerFifo(){
    
    //open server fifo
    if ((server_fd = open(serverPid, O_RDONLY )) == -1){
        perror("Failed to open server fifo.");
        exit(EXIT_FAILURE);        
    }

    //read client struct from server fifo
    struct BiboClient biboClient;
    if(read(server_fd, &biboClient, sizeof(biboClient))== -1){
        perror("Failed read server fifo.");
        exit(EXIT_FAILURE);        
    }

    //close server fifo
    if(close(server_fd)== -1){
        perror("Failed to close server fifo.");
        exit(EXIT_FAILURE);        
    }

    //send SIGINT signal to server if client gets SIGINT signal
    if(strcmp(biboClient.connectInfo, "kill") == 0){
        char* id = findClientId(biboClient.clientPid);
        printf("kill signal from %s. terminating.\n", id );
        free(id);
        signal_handler(SIGINT);
    }

    return biboClient;

}



//client's help command
char* help_command(int key, char* command){
    char* response = "";

    //give the command list
    if(key == 1){
        response = "\tAvailable comments are :\nhelp, list, readF, writeT, upload, download, quit, killServer\n";
    }
    //give the details for required command
    else if(key == 2){

        if(strcmp(command, "help") == 0){
            response = "Display the list of possible client requests.\n";
        }
        else if(strcmp(command, "list") == 0){
            response = "Sends a request to display the list of files in Servers directory(also displays the list received from the Server).\n";
        }
        else if(strcmp(command, "readF") == 0){
            response = "readF <file> <line #>\nRequests to display the # line of the <file>, if no line number is given the whole contents of the file is requested (and displayed on the client side).\n";
        }
        else if(strcmp(command, "writeT") == 0){
            response = "writeT <file> <line #> <string>\nRequest to write the content of 'string' to the #th line the <file>, if the line # is not given writes to the end of file. If the file does not exists in Servers directory creates and edits the file at the same time.\n";
        }
        else if(strcmp(command, "upload") == 0){
            response = "upload <file>\nUploads the file from the current working directory of client to the Servers directory (beware of the cases no file in clients current working directory and file with the same name on Servers side).\n";
        }
        else if(strcmp(command, "download") == 0){
            response = "download <file>\nRequest to receive <file> from Servers directory to client side.\n";
        }
        else if(strcmp(command, "quit") == 0){
            response = "quit\nSend write request to Server side log file and quits.\n";
        }
        else if(strcmp(command, "killServer") == 0){
            response = "killServer\nSends a kill request to the Server.\n";
        }
        else {
            response = "Command is not supported.\nAvailable comments are :\nhelp, list, readF, writeT, upload, download, quit, killServer.\n";
        }
    }
    return response;
}

//client's quit command
char* quit_command(){
    char* response = "logfile write request granted.\nbye.\n";
    return response;

}

//client's list command
char* list_command(){


    char path[20] = "./";
    strcat(path, biboServer.dirName);

    DIR *dir;
    struct dirent *ent;

    dir = opendir(path);
    if (dir == NULL) {
        perror("Error opening directory.\n");
        return NULL;
    }

    char* file_list = (char*)calloc(MAX_COMMAND_BUFFER, sizeof(char));
    strcpy(file_list, "");
    if (file_list == NULL) {
        perror("Memory allocation failed.\n");
        closedir(dir);
        return NULL;
    }

    size_t file_list_size = 0;
    while ((ent = readdir(dir)) != NULL) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) {
            continue; // skip current directory and parent directory entries
        }
        size_t name_len = strlen(ent->d_name);
        if (file_list_size + name_len + 1 >= MAX_COMMAND_BUFFER) {
            perror("File list exceeds buffer size.\n");
            break;
        }
        strcat(file_list, ent->d_name);
        strcat(file_list, "\n");
        file_list_size += name_len + 1;
    }
    closedir(dir);
    return file_list;
}

//client's upload-download command
char* load_command(char* sourcePath, char* destinationPath){

    char destination_file_name_new[255];
    static char response[MAX_COMMAND_BUFFER] = {0};
    
    FILE *source_file = fopen(sourcePath, "rb");
    if (source_file == NULL) {
        strcpy(response, "Given file name is not found.\n");
        return response;
    }
    
    // Check if file exists at destination
    FILE *destination_file = fopen(destinationPath, "rb");
    if (destination_file != NULL) {
        strcpy(response, "File is already exist at the source path.\n");
        return response;
    }
    
    
    FILE *new_file = fopen(destinationPath, "wb");
    if (new_file == NULL) {
        strcpy(response, "Error creating the new file.\n");
        return response;
    }
    
    // Copy contents of source file to new file
    char buffer[1024];
    size_t bytes_read;
    size_t total_bytes_read = 0;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), source_file)) > 0) {
        total_bytes_read += bytes_read;
        fwrite(buffer, 1, bytes_read, new_file);
    }
    
    fclose(source_file);
    fclose(new_file);

    sprintf(response, "file transfer request received. Beginning file transfer:\n %ld bytes transferred.\n", total_bytes_read);
    return response;
    
}

//client's readF command
char* read_command(char* filename, int num_line){

    char* response = (char*) calloc(MAX_COMMAND_BUFFER, sizeof(char));
    strcpy(response, "");


    FILE* file = fopen(filename, "r");
    if (file == NULL) {
        strcpy(response, "Error opening the file.\n");
        return response;
    }
    // Check if line number is provided
    if (num_line != -1) {
        int current_line = 1;
        char* line = NULL;
        size_t len = 0;
        ssize_t read;

        while ((read = getline(&line, &len, file)) != -1) {
            if (current_line == num_line) {
                fclose(file);
                return line;
            }
            current_line++;
        }

        fclose(file);
        strcpy(response,"");
        strcpy(response, "Given line number not found.\n");
        return response;
    } 
    
    else {

        // Read the whole file
        struct stat file_info;
        if (fstat(fileno(file), &file_info) == -1) {
            fclose(file);
            strcpy(response,"Error getting file information.\n");
            return response;
        }

        size_t file_size = file_info.st_size;
        char* contents = (char*)calloc(file_size, sizeof(char));
        fread(contents, file_size, 1, file);
        contents[file_size] = '\0';

        fclose(file);
        return contents;
    }
}

//client's writeT command
char* write_command(char* filename, char* str, int lineNumber) {
    

    static char response[MAX_COMMAND_BUFFER] = {0};
    strcpy(response, "");
    
    // Open the file in read mode
    FILE* file = fopen(filename, "r");
    if (file == NULL) {
       strcpy(response, "Error opening the file.\n");
        return response;
    }

    // Create a temporary file to write the modified content
    char* tempFilename = "tempFileEzgi";
    FILE* tempFile = fopen(tempFilename, "w");
    if (tempFile == NULL) {
        strcpy(response, "Error creating temporary file.\n");
        fclose(file);
        return response;
    }

    // Copy the lines from the original file to the temporary file
    char buffer[1024];
    int currentLineNumber = 1;
    while (fgets(buffer, sizeof(buffer), file) != NULL) {
        if (lineNumber == -1 || currentLineNumber != lineNumber) {
            fputs(buffer, tempFile);
        } else {
            // Insert the string at the current line
            fputs(str, tempFile);
            fputs(buffer, tempFile);
        }
        currentLineNumber++;
    }


    // If the lineNumber is greater than the total number of lines, insert the string at the end
    if (lineNumber == -1 || lineNumber > currentLineNumber) {
        fputs(str, tempFile);
        sprintf(response,"Line '%s' is written at the end of the %s file.", str, filename);
    } else {
        sprintf(response,"Line '%s' is written to %d. line of the %s file.", str, lineNumber, filename);
    }


    fclose(file);
    fclose(tempFile);

    // Remove the original file
    remove(filename);

    // Rename the temporary file to the original filename
    if (rename(tempFilename, filename) != 0) {
        strcpy(response, "Error renaming temporary file.\n");
        return response;
    }

    return response;
}





//to find client id (client01..etc) according to it client pid
//searches in client queue
char* findClientId(char* clientPid){
    int pid = atoi(clientPid);
    char id[20]; // Assuming the string can hold up to 20 characters
    char* clientId = malloc(20); // Allocate memory for clientId

    if (clientId == NULL) {
        // Handle allocation failure
        return NULL;
    }

    strcpy(clientId, "client");

    for (int i = 0; i < sharedMemory->clientCount; i++) {
        if (clientPids[i] == pid) {
            sprintf(id, "%d", i+1);
            if (i < 10) {
                strcat(clientId, "0");
            }
            strcat(clientId, id);
            return clientId;
        }
    }

    // If clientId is not found, return an empty string
    strcpy(clientId, "");
    return clientId;
}

//signal handler for server 
void signal_handler(int signal) {
    if(signal == SIGINT){
        printf("\nSIGINT signal is catched.\n");
        keepRunning = 0;
        freeResources();
        exit(EXIT_SUCCESS);
    }
   
}

//signal handler for child 
void signal_handler_child(int signal){
    //
}

///function to get arguments
int getArguments(int argc, char **argv){
    
    //check that is there enough console arguments
    if(argc != 4){
        perror("Usage: There should be 4 console arguments. [biboServer <dirname> <maxNumOfClients> <poolSize>]");
        return -1;
    }
    
    //store connection info and server pid
    strcpy(biboServer.dirName, argv[1]);
    biboServer.maxClient = atoi(argv[2]);
    biboServer.poolSize = atoi(argv[3]);

    return 0;
}

//frees all allocated resources 
void freeResources(){

    fclose(tempFile);
    remove(TEMP_FILE_PATH);
    
    //remove server double inst checker temp file
    fclose(log_fd);
    closedir(d);

    //free fifos
    close(server_fd);
    unlink(serverPid);
    remove(serverPid);

    for(int i=0; i<biboServer.poolSize; i++){
        pthread_detach(threadPids[i]);
    }

    for(int i=0; i<sharedMemory->clientCount; i++){
        kill(clientPids[i], SIGINT);
    }
 

    if (sem_destroy(&(sharedMemory->sem_clients)) == -1) {
        perror("Failed to destroy semaphore.\n");
        exit(EXIT_FAILURE);
    }
    if (sem_destroy(&(sharedMemory->sem_queue)) == -1) {
        perror("Failed to destroy semaphore.\n");
        exit(EXIT_FAILURE);
    }
    if (sem_destroy(&(sharedMemory->sem_logfile)) == -1) {
        perror("Failed to destroy semaphore.\n");
        exit(EXIT_FAILURE);
    }
    if (sem_destroy(&(sharedMemory->readTry)) == -1) {
        perror("Failed to destroy semaphore.\n");
        exit(EXIT_FAILURE);
    }
    if (sem_destroy(&(sharedMemory->rmutex)) == -1) {
        perror("Failed to destroy semaphore.\n");
        exit(EXIT_FAILURE);
    }
    if (sem_destroy(&(sharedMemory->rsc)) == -1) {
        perror("Failed to destroy semaphore.\n");
        exit(EXIT_FAILURE);
    }
    if (sem_destroy(&(sharedMemory->wmutex)) == -1) {
        perror("Failed to destroy semaphore.\n");
        exit(EXIT_FAILURE);
    }

    //free shared memories
    if(munmap(sharedMemory, sizeof(struct SharedMemory)) == -1) {
        perror("Failed to unmap shared memory.\n");
        exit(EXIT_FAILURE);
    }
    if(shm_unlink("sharedMem") == -1) {
        perror("Failed to unlink shared memory object.\n");
        exit(EXIT_FAILURE);
    }
    if(munmap(clientQueue, sizeof(struct BiboClient) * CLIENT_SIZE) == -1) {
        perror("Failed to unmap shared memory.\n");
        exit(EXIT_FAILURE);
    }
    if(shm_unlink("queueMem") == -1) {
        perror("Failed to unlink shared memory object.\n");
        exit(EXIT_FAILURE);
    }

    pthread_mutex_destroy(&empty_queue);
   
}

  
  

