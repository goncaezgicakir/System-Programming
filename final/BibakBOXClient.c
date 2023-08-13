//GONCA EZGI CAKIR 
//151044054
//CSE344 FINAL PROJECT - BibakBOXClient

#include "BibakBOXClient.h"


int main(int argc, char **argv){

    //set while loop control variable
    keepRunning=1;

    //controller for removing files
    removeControl = 0;

    //signal handling----------------------------------------------------------------------------------
    struct sigaction act;
    memset(&act, 0, sizeof(act));
    act.sa_handler = signal_handler;
    act.sa_flags = 0;
    sigaction(SIGINT, &act, NULL);

    printf("CLIENT SIDE\n");

    //get console arguments ----------------------------------------------------------------------------
    if(getArguments(argc, argv) == -1){
        exit(EXIT_FAILURE);
    }

    //store client directory name in its info struct
    strcpy(client.dirPath, "");
    strcpy(client.dirPath, dirName);

    //create log file  and open it for logging--------------------------------------------------------
    printf("Logging...\n");
    log_filename = (char*)malloc(256 * sizeof(char));         
    strcpy(log_filename, "");
    sprintf(log_filename,"%s/logfile", clientMainDir);
    log_fd = open(log_filename, O_WRONLY | O_CREAT | O_APPEND, 0644);


    //CREATE CLIENT SOCKET------------------------------------------------------------------------------
    clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket == -1) {
        perror("Client socket failed.");
        exit(EXIT_FAILURE);
    }

    //get client IP address,then store it
    strcpy(clientIP, "");
    strcpy(clientIP, getLocalIP());
    strcpy(client.clientIP, "");
    strcpy(client.clientIP, clientIP);

    //set client pid
    client.pid = getpid();

    //set client address
    memset(&clientAddress, 0, sizeof(clientAddress));
    clientAddress.sin_family = AF_INET;
    clientAddress.sin_addr.s_addr = INADDR_ANY;
    clientAddress.sin_port = htons(serverPort);

    //CONNECT SERVER SOCKET---------------------------------------------------------------------------
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) {
        perror("Server socket failed.");
        exit(EXIT_FAILURE);
    }

    //set up the server address structure
    memset(&serverAddress, 0, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(serverPort);
    if (inet_pton(AF_INET, serverIP, &(serverAddress.sin_addr)) <= 0) {
        perror("Invalid IP address for server.");
        exit(EXIT_FAILURE);
    }


    //connect to the server
    if (connect(serverSocket, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0) {
        perror("Connection to server socket failed.");
        exit(EXIT_FAILURE);
    }

    //send client information to server
    if (send(serverSocket, &client, sizeof(ClientInfo), 0) == -1) {
        perror("Sending clientInfo with to server failed.");
        exit(EXIT_FAILURE);
    }
    
    //CLIENT STARTS------------------------------------------------------------------------------

    while(keepRunning){

        //GET SERVER CONTENT------------------------------------------------------------------------------------
        //receive server file content from server
        //allocate space for the server content 
        FileInfo* serverFileContent = calloc(MAX_FILE, sizeof(FileInfo));
        initFileContentArray(serverFileContent);

        FileInfo temp_A;

        for(int i=0; i<MAX_FILE; i++){
            
            //receive server file content
            if (recv(serverSocket, &temp_A, sizeof(FileInfo), 0) == -1) {
                perror("Receiving server file content failed.");
                close(serverSocket);
                exit(EXIT_FAILURE);
            }

            //end notify received break the loop
            if(temp_A.type == 3){
                break;
            }

            //store the file content
            serverFileContent[i] = temp_A;
        }

        //printf("\n\nSERVER CONTENT\n------------------\n");
        //printFileContentArray(serverFileContent);


        //SEND DIFFERENCES TO SERVER---------------------------------------------------------------------------------------
        //get differences from server content then our content
        //send these difference file content to server

        FileInfo* clientFileContent = calloc(MAX_FILE, sizeof(FileInfo));
        initFileContentArray(clientFileContent);
        int clientContentSize = 0;
        clientFileContent = traverseDirectory(dirName, clientFileContent, &clientContentSize, 2);

        int serverContentSize = getFileContentSize(serverFileContent);
        FileInfo* fileContentDifferences = calloc(MAX_FILE, sizeof(FileInfo));
        initFileContentArray(fileContentDifferences);

        /*if(removeControl == 1){
            FileInfo* removeDifferences = calloc(MAX_FILE, sizeof(FileInfo));
            initFileContentArray(removeDifferences);
            removeDifferences = findDifferences(clientFileContent, serverFileContent, clientContentSize, serverContentSize);
            int removeDifferencesSize = getFileContentSize(removeDifferences);
            
            printf("\n\n REMOVE CONTENT\n------------------\n");
             printFileContentArray(removeDifferences);

            for(int i=0; i<removeDifferencesSize; i++){
                remove(removeDifferences[i].path);
                printf("---\n\n REMOVED %s -----\n\n", removeDifferences[i].path);
            }

            free(removeDifferences);
        }

        //update client file content
        free(clientFileContent);
        clientFileContent = calloc(MAX_FILE, sizeof(FileInfo));
        initFileContentArray(clientFileContent);
        clientContentSize = 0;
        clientFileContent = traverseDirectory(dirName, clientFileContent, &clientContentSize, 2);*/

        //store differences
        fileContentDifferences = findDifferences(serverFileContent, clientFileContent, serverContentSize, clientContentSize);
        
        //printf("\n\n MY DIFFERENCE CONTENT\n------------------\n");
        //printFileContentArray(fileContentDifferences);

        int differenceContentSize = getFileContentSize(fileContentDifferences);
        
        //send difference content file array to server to notify the missing files
        for(int i=0; i<differenceContentSize; i++){
            
            //send file information to current client
            if (send(serverSocket, &(fileContentDifferences[i]), sizeof(FileInfo), 0) == -1) {
                perror("Sending difference FileInfo to server failed.");
                exit(EXIT_FAILURE);
            }
        }

        //send the end notify (difference) to server
        FileInfo endNotify_D;
        endNotify_D.type = 3;
        if (send(serverSocket, &endNotify_D, sizeof(FileInfo), 0) == -1) {
            perror("Sending endNotify(difference) struct to server failed.");
            exit(EXIT_FAILURE);
        }
        
        //GET MISSING FILE CONTENT FROM SERVER-------------------------------------------------------------------------
        for(int i=0; i<differenceContentSize; i++){
            
            char* filename = (char*)malloc(256 * sizeof(char));         
            strcpy(filename, "");
        
            sprintf(filename,"%s/%s", clientMainDir, fileContentDifferences[i].path);
            //if the file type is 1 (FILE)
            if(fileContentDifferences[i].type == 1){

                FILE* file = fopen(filename, "w+");
                if (file == NULL) {
                    perror("Error opening file");
                    exit(EXIT_FAILURE);
                }

                char ch;
                for (int j=0; j<fileContentDifferences[i].bytesRead; j++) {
                    recv(serverSocket, &ch, sizeof(char), 0);
                    fputc(ch, file);  // Write each received character into the file
                }
                
                //add log
                pthread_mutex_unlock(&logfile_mutex);
                char *log = (char*)calloc(MAX_LOG_LENGTH , sizeof(char));         
                strcpy(log, "");
                sprintf(log,"File %s is added/modified by server.\n", fileContentDifferences[i].name);
                log[strlen(log)] = '\0';
                write(log_fd, log, strlen(log));
                free(log);
                pthread_mutex_unlock(&logfile_mutex);

                //close the file
                fclose(file);
            }

            else if(fileContentDifferences[i].type == 2){
                mkdir(filename, 0777);
                //add log
                pthread_mutex_unlock(&logfile_mutex);
                 char *log = (char*)calloc(MAX_LOG_LENGTH , sizeof(char));         
                strcpy(log, "");
                sprintf(log,"Dir %s is added/modified by server.\n", fileContentDifferences[i].name);
                log[strlen(log)] = '\0';
                write(log_fd, log, strlen(log));
                free(log);
                pthread_mutex_unlock(&logfile_mutex);
            }

            free(filename);

        }
        
        free(clientFileContent);
        free(serverFileContent);
        free(fileContentDifferences);

        //SEND CONTENT TO SERVER----------------------------------------------------------------------------------------
        //send client content to server

        clientFileContent = calloc(MAX_FILE, sizeof(FileInfo));
        clientContentSize = 0;
        initFileContentArray(clientFileContent);
        clientFileContent = traverseDirectory(dirName, clientFileContent, &clientContentSize, 2);

        for(int i=0; i<clientContentSize; i++){
            
            //send file information to current client
            if (send(serverSocket, &(clientFileContent[i]), sizeof(FileInfo), 0) == -1) {
                perror("Sending FileInfo to server failed.");
                exit(EXIT_FAILURE);
            }

        }

        //send the end notify(all) to server
        FileInfo endNotify_A;
        endNotify_A.type = 3;
        if (send(serverSocket, &endNotify_A, sizeof(FileInfo), 0) == -1) {
            perror("Sending endNotify(all) struct to server failed.");
            exit(EXIT_FAILURE);
        }

        //printf("\n\nCLIENT CONTENT\n------------------\n");
        //printFileContentArray(clientFileContent);

        //GET SERVER NEEDED CONTENT------------------------------------------------------------------------------------
        //receive server difference file content from server
        //allocate space for the server difference content 

        FileInfo* serverFileDifferenceContent = calloc(MAX_FILE, sizeof(FileInfo));
        initFileContentArray(serverFileDifferenceContent);
        //printFileContentArray(serverFileDifferenceContent);
        FileInfo temp_D;

        for(int i=0; i<MAX_FILE; i++){
            
            //receive server file content
            if (recv(serverSocket, &temp_D, sizeof(FileInfo), 0) == -1) {
                perror("Receiving server file content failed.");
                close(serverSocket);
                exit(EXIT_FAILURE);
            }

            //end notify received break the loop
            if(temp_D.type == 3){
                break;
            }

            //store the file content
            serverFileDifferenceContent[i] = temp_D;
        }

        //printf("\n\nSERVER DIFFERENCE CONTENT\n------------------\n");
        //printFileContentArray(serverFileDifferenceContent);


        //SEND SERVER'S NEEDED FILES TO SERVER-----------------------------------------------------------------------------------
        int serverContentDifferenceSize = getFileContentSize(serverFileDifferenceContent);
        
        for(int i=0; i<serverContentDifferenceSize; i++){
            
            //if the file type is 1 (FILE)
            if(serverFileDifferenceContent[i].type == 1){
                char* filename = (char*)malloc(256 * sizeof(char));         
                strcpy(filename, "");
                sprintf(filename,"%s/%s", clientMainDir, serverFileDifferenceContent[i].path);
                printf("file:'%s'\n", filename);
            
                FILE* file = fopen(filename, "rb");
                if (file == NULL) {
                    perror("Error opening file");
                    exit(EXIT_FAILURE);
                }

                char ch;
                for (int j=0; j<serverFileDifferenceContent[i].bytesRead; j++) {
                    ch = fgetc(file);
                    send(serverSocket, &ch, sizeof(char), 0);  // Send each character through the socket
                }

                // Close the file and socket
                fclose(file);
                free(filename);
            }
        }

        free(serverFileDifferenceContent);
        free(clientFileContent);

        removeControl = 1;
    }
    
    return 0;

}



//get client machine local IP address
//TODO: implement this
char* getLocalIP(){
       return "127.0.0.1";

}


//signal handler for SIGINT
void signal_handler(int signal) {
    if(signal == SIGINT){
        printf("\n------SIGINT signal is catched------\n");
        keepRunning=0;
        freeResources();
        exit(EXIT_SUCCESS);
    }
}


//function to get arguments
int getArguments(int argc, char **argv){
    
     //check that is there enough console arguments
    if(argc != 4 && argc != 3){
        perror("Usage: There should be 3 or 4 console arguments. [ BibakBOXServer <dirName> <portnumber> <server_address> ; <server_address> is optinal.] ");
        return -1;
    }
    

    //store arguments
    strcpy(dirName, "");
    strcpy(dirName, argv[1]);
    serverPort = atoi(argv[2]);

    strcpy(serverIP, "");
    if(argv[3] == NULL){
        strcpy(serverIP, "127.0.0.1");
    } else {
        strcpy(serverIP, argv[3]);
    }

    //store the main directory name of the client
    strcpy(clientMainDir, getFileName(dirName));
    //printf("dir:%s  port:%d IP:%s\n", dirName, serverPort, serverIP);

    return 0;
}


//free all allocated spaces
void freeResources(){

    close(clientSocket);
    free(log_filename);
    pthread_mutex_destroy(&logfile_mutex);

}







