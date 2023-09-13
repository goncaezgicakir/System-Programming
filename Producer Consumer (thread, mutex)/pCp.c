//GONCA EZGI CAKIR 
//151044054
//CSE344 hw5 - pCp

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <sys/ipc.h>
#include <dirent.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <sys/time.h>

#define MAX_PATH_LENGTH 1024

//struct of buffer item for consumers
struct BufferItem{

    char filename[MAX_PATH_LENGTH];
    char source[MAX_PATH_LENGTH];
    char destination[MAX_PATH_LENGTH];
    int sourceFd;
    int destinationFd;
};


//globals
int bufferSize;
int numConsumers;
char paths[2][MAX_PATH_LENGTH];
pthread_t producerTid;
pthread_t* consumerTids;
struct BufferItem* buffer;
int currentSize;


//mutex and condition variables
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
sem_t empty;
sem_t full;
sem_t print;
int done;

//statistic vairables
int numDirectory;
int numFifo;
int numFile;
int totalBytesLoaded;
struct timeval startTime, endTime;

//funtion definitions
void signal_handler(int signal);
int getArguments(int argc, char **argv);
void freeResources();
void printStatistics();

void* producer(void* arg);
void* consumer(void* arg);
void listFiles(const char *path, const char *destinationPath) ;
int loadFile(struct BufferItem item);

struct BufferItem removeItem();
struct BufferItem* addItem(struct BufferItem item);
struct BufferItem* initBuffer();
void printBuffer();



//MAIN--------------------------------------------------------------------------------------------------------------------------------------------------
int main(int argc, char **argv){

    //signal handling---------------------------------------------------------------------------
    struct sigaction act;
    memset(&act, 0, sizeof(act));
    act.sa_handler = signal_handler;
    act.sa_flags = 0;
    sigaction(SIGINT, &act, NULL);   
    
    //initialize globals
    done = 0;
    numDirectory = 0;
    numFile = 0;
    numFifo = 0;
    totalBytesLoaded = 0;

    //get arguments fails-----------------------------------------------------------------------
    if(getArguments(argc, argv) == -1){
        return -1;
    }

    //initialize arrays
    buffer = (struct BufferItem *)malloc(bufferSize * sizeof(struct BufferItem));
    consumerTids = (pthread_t *)malloc(numConsumers * sizeof(pthread_t));
    
    //create the buffer
    buffer = initBuffer();
    printf("Buffer created with size %d.\n", bufferSize);

    //INIT SEMAPHORES----------------------------------------------------------------------
    if (sem_init(&empty, 1, bufferSize) == -1){
        perror("Failed to create empty(buffer size) semaphore.\n");
        exit(EXIT_FAILURE);
    }

    if (sem_init(&full, 1, 0) == -1){
        perror("Failed to create full semaphore.\n");
        exit(EXIT_FAILURE);
    }

    if (sem_init(&print, 1, 1) == -1){
        perror("Failed to create print semaphore.\n");
        exit(EXIT_FAILURE);
    }
    //CREATING THREADS ---------------------------------------------------------------------
    printf("Thread pool is creating with size %d.\n", numConsumers);

    // Get the start time
    gettimeofday(&startTime, NULL);

    //create the thread pool
    //create producer thread
    pthread_create(&producerTid, NULL, producer, (void *)paths);

   //create consumer threads
    for (int i = 0; i < numConsumers; i++) {
        int* t_id = (int*)malloc(sizeof(int));
        *t_id = i;
        pthread_create(&consumerTids[i], NULL, consumer, t_id);
    }
    

    freeResources();
    printStatistics();

    return 0;
}




//PRODUCER - CONSUMER FUNCTIONS--------------------------------------------------------------------------------------------------------------------------
//thread function for producer
void *producer(void *arg) {

    //get source and destination paths
    char (*paths)[MAX_PATH_LENGTH] = (char (*)[MAX_PATH_LENGTH])arg;
    char *sourcePath = paths[0];
    char *destinationPath = paths[1];

    
    //new entry path for this iteration of the recursion
    struct stat st;
    if (stat(destinationPath, &st) == -1) {
        
        //directory is not exist, then create
        if (mkdir(destinationPath, 0777) == -1) {
            perror("Failed to create source directory.\n");
        } else {
            sem_wait(&print);
            printf("Directory '%s' is created. [%lld bytes]\n", destinationPath, (long long) st.st_size);
            numDirectory = numDirectory + 1; //increment directory count for statistics
            totalBytesLoaded = totalBytesLoaded + st.st_size;   //update the total loaded bytes for statistics
            sem_post(&print);
        }
    }

    //find the last directory of the given source path 
    //in order to create this at the destination path
    char *myDir;
    char temp[128];
    char *myLastDir = NULL;
    temp[0] = '\0';
    strcpy(temp, sourcePath);
    myDir = strtok(temp, "/");
    while (myDir != NULL) {
        myLastDir = myDir;
        myDir = strtok(NULL, "/");
    }

    //create the new destination path (cp -R functionality works like this)
    char mainDestinationPath[128];
    mainDestinationPath[0] = '\0';
    strcpy(mainDestinationPath, "");
    sprintf(mainDestinationPath, "%s/%s", destinationPath, myLastDir);

    //if this new destination path directory doesn't exist, create it 
    if (access(mainDestinationPath, F_OK) != 0) {
        if (mkdir(mainDestinationPath, 0777) != 0) {
            perror("Failed to create main directory.\n");
        } 
        else {
            struct stat st;
            stat(myLastDir, &st);
            sem_wait(&print);
            printf("Directory '%s' is created. [%lld bytes]\n", myLastDir, (long long)st.st_size);
            numDirectory = numDirectory + 1; //increment directory count for statistics
            totalBytesLoaded = totalBytesLoaded + st.st_size;   //update the total loaded bytes for statistics
            sem_post(&print);
        }
    }

    //store the new destination path
    strcpy(destinationPath, mainDestinationPath);

    sem_wait(&print);
    printf("Producer is created.\n");
    sem_post(&print);

    //search all files in source path
    listFiles(sourcePath, destinationPath);

    //set done flag to finish consumers
    done = 1;

    sem_wait(&print);
    printf("Producer finished.\n");
    sem_post(&print);

    return NULL;
}

//function to search through all files recursively at the given source path
//it creates directories and fifos at the destination path
//adds the file information as an item to buffer in order them to consumed by consumer threads later on
void listFiles(const char *path, const char *destinationPath) {
    
    DIR *dir;
    struct dirent *entry;

    //search for source path content if it is exist
    if ((dir = opendir(path)) != NULL) {

        while ((entry = readdir(dir)) != NULL) {

            //base case for recursion
            if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
                
                //new entry path for this iteration of the recursion
                char entryPath[MAX_PATH_LENGTH];
                strcpy(entryPath, "");
                sprintf(entryPath, "%s/%s", path, entry->d_name);
                //new destination path for this iteration of the recursion
                char destinationEntryPath[MAX_PATH_LENGTH];
                strcpy(destinationEntryPath, "");
                sprintf(destinationEntryPath, "%s/%s", destinationPath, entry->d_name);

                //check file stat to determine fifos
                struct stat fileStat;
                if (stat(destinationEntryPath, &fileStat) == 0) {

                    //if it is a fifo, create a new one at the destination
                    if (S_ISFIFO(fileStat.st_mode)) {
                        umask(0);
                        int errno;
                        if ((errno = mkfifo(destinationEntryPath, S_IRUSR | S_IWUSR | S_IWGRP) == 1) && (errno != EEXIST)){
                            perror("Fifo is already exist.");
                        }
                        sem_wait(&print);
                        printf("Fifo '%s' is loaded. [%lld bytes]\n", entry->d_name, (long long) fileStat.st_size);
                        numFifo = numFifo + 1; //increment fifo counter for statistics
                        totalBytesLoaded = totalBytesLoaded + fileStat.st_size;   //update the total loaded bytes for statistics
                        sem_post(&print);
                        continue;
                    }
                }

                //if it is a new entry is a directory
                if (entry->d_type == DT_DIR) {

                    // recursive call listFiles with subdirectory
                    // create the subdirectory 
                    mkdir(destinationEntryPath, 0777);

                    struct stat st;
                    stat(destinationEntryPath, &st);

                    sem_wait(&print);
                    printf("Directory '%s' is created. [%lld bytes]\n", entry->d_name, (long long)st.st_size);
                    numDirectory = numDirectory + 1; //increment the directory count for statistics
                    totalBytesLoaded = totalBytesLoaded + st.st_size;   //update the total loaded bytes for statistics
                    sem_post(&print);

                    listFiles(entryPath, destinationEntryPath);
                } 


                //if it is a file then store content (path, file descriotor)
                else if (entry->d_type == DT_REG) {

                    //new file item for the buffer
                    struct BufferItem item;
                    sprintf(item.source, "%s/%s", path, entry->d_name);
                    sprintf(item.destination, "%s/%s", destinationPath, entry->d_name);
                    strcpy(item.filename, entry->d_name);

                    // open source file
                    int sourceFd = open(item.source, O_RDONLY);
                    if (sourceFd == -1) {
                        perror("Failed to open source file.\n");
                        continue;
                    }
                    //store source file descriptor
                    item.sourceFd = sourceFd;

                    // open destination file with truncation
                    int destinationFd = open(item.destination, O_RDWR | O_CREAT | O_TRUNC, 0666);
                    if (destinationFd == -1) {
                        perror("Failed to open destination file.\n");
                        close(sourceFd);
                        continue;
                    }
                    //store destination file descriptor
                    item.destinationFd = destinationFd;

                    //sync-------------------------------------------------------------------------
                    sem_wait(&empty);
                    pthread_mutex_lock(&mutex);

                    //add the file item to buffer in order to handle it by consumers
                    addItem(item);
                
                    pthread_mutex_unlock(&mutex);
                    sem_post(&full);
                    //-----------------------------------------------------------------------------
                }
            }
        }
        closedir(dir);
    } else {
        perror("Source path couldn't be opened.\n");
    }
}

//thread function for consumer
void *consumer(void *arg) {

    //get thread number
    int* t_id = (int*)arg;
    int id = *t_id;
    free(arg);

    sem_wait(&print);
    printf("Consumer %d is created.\n", id);
    sem_post(&print);

    //consume the buffer item(file) and load its content
    //till producer sets the done flag to 1
    while (currentSize > 0 || done == 0) {

        //sync-----------------------------------------------------
        sem_wait(&full);
        pthread_mutex_lock(&mutex);
        //---------------------------------------------------------

       
        if (currentSize == 0  && done == 1) {
            sem_post(&full);
            pthread_mutex_unlock(&mutex);
            break;
        }

        //get an item from buffer to consume
        struct BufferItem item = removeItem();

        //sync-----------------------------------------------------
        pthread_mutex_unlock(&mutex);
        sem_post(&empty);
        //---------------------------------------------------------

        //load file content
        int loadedByte = loadFile(item);  
        sem_wait(&print);
        printf("Consumer %d loaded the file %s. [%d bytes]\n", id, item.filename, loadedByte);
        numFile = numFile + 1;  //increment file counter for statistic
        totalBytesLoaded = totalBytesLoaded + loadedByte;   //update the total loaded bytes for statistics
        sem_post(&print);


        if (currentSize == 0 && done == 1) {
            sem_post(&full);
            pthread_mutex_unlock(&mutex);
            break;
        }

    }
    sem_wait(&print);
    printf("Consumer %d is finished.\n", id);
    sem_post(&print);

    return NULL;
}

//function to load source file content to destination file
int loadFile(struct BufferItem item) {

    //get file descriptors
    int sourceFd = item.sourceFd;
    int destinationFd = item.destinationFd;

    //content buffer
    char buffer[2048];
    ssize_t bytes_read, bytes_written;
    off_t total_bytes_read = 0;

    //read from sourceFd and write to destinationFd
    while ((bytes_read = read(sourceFd, buffer, sizeof(buffer))) > 0) {
        total_bytes_read += bytes_read;
        bytes_written = write(destinationFd, buffer, bytes_read);
    }

    //close all files
    close(item.sourceFd);
    close(item.destinationFd);

    return total_bytes_read;
}



//HELPER FUNCTIONS---------------------------------------------------------------------------------------------------------------------------------------
//signal handler for SIGINT  
void signal_handler(int signal) {
    if(signal == SIGINT){
        printf("\n------------ SIGINT signal is catched ------------\n");
        
        //shut done threads my their tids
        pthread_detach(producerTid);

        for (int i = 0; i < numConsumers; i++) {
            pthread_detach(consumerTids[i]);
        }

        //free allocated spaces
        free(buffer);
        free(consumerTids);

        //destroy semaphores
        sem_destroy(&empty);
        sem_destroy(&full);
        sem_destroy(&print);
        exit(EXIT_SUCCESS);
    }
   
}

///function to get console arguments
int getArguments(int argc, char **argv){
    
    //check that is there enough console arguments
    if(argc != 5){
        perror("Usage: There should be 5 console arguments. [pcp <bufferSize> <numOfConsumer> <sourcePath> <destinationPath>]");
        return -1;
    }
    
    //store buffer size, number of consumers, source path and destination path
    bufferSize = atoi(argv[1]);
    numConsumers = atoi(argv[2]);

    if(bufferSize <= 0){
        perror("Usage: Buffer size has to be greater than zero.\n");
        return -1;
    }

    else if(numConsumers <= 0){
        perror("Usage: Number of consumer has to be greater than zero.\n");
        return -1;
    }

    strcpy(paths[0], argv[3]);
    strcpy(paths[1], argv[4]);

    return 0;
}

//frees all allocated resources 
void freeResources(){
    
    //shut done threads my their tids
    pthread_join(producerTid, NULL);

    for (int i = 0; i < numConsumers; i++) {
        pthread_join(consumerTids[i], NULL);
    }

    //free allocated spaces
    free(buffer);
    free(consumerTids);

    //destroy semaphores
    sem_destroy(&empty);
    sem_destroy(&full);
    sem_destroy(&print);

    gettimeofday(&endTime, NULL);
   
}

//prints loaded files,fifo and directory counts
void printStatistics(){

    sem_wait(&print);
    printf("\n\n------------ SUMMARY ------------\n");
    printf("Number of loaded fifo -> %d\n", numFifo);
    printf("Number of loaded directory -> %d\n", numDirectory);
    printf("Number of loaded file -> %d\n", numFile);
    printf("Total bytes loaded -> %d\n", totalBytesLoaded);

    double totalTime = (endTime.tv_sec - startTime.tv_sec) * 1000.0; // seconds to milliseconds
    totalTime += (endTime.tv_usec - startTime.tv_usec) / 1000.0; // microseconds to milliseconds

    printf("Total time taken: %f ms\n", totalTime);

    printf("-----------------------------------\n\n");
    sem_post(&print);
    return;

}

//BUFFER QUEUE FUNCTIONS---------------------------------------------------------------------------------------------------------------------------------
//initialize  buffer and current size
struct BufferItem* initBuffer() {
    
    int i;
    currentSize=0;
    for (i = 0; i < bufferSize; i++) {
        strcpy(buffer[i].filename, "X");
        strcpy(buffer[i].source, "X");
        strcpy(buffer[i].destination, "X");
        buffer[i].sourceFd = -5;
        buffer[i].destinationFd = -5;
    }

    return buffer;
}

//add new item to the end of buffer 
struct BufferItem* addItem(struct BufferItem item){
    
    int idx;

    for (idx = 0; idx < bufferSize; idx++) {

        //last element has found
        if(strcmp(buffer[idx].filename,"X") == 0){
            buffer[idx] = item;
            currentSize = currentSize +1; //increment buffer's current size
            return buffer;
        }
    }
}

//remove the first item from buffer
struct BufferItem removeItem() {
    
    int i;
    struct BufferItem item = buffer[0]; 
    
    //shift items
    for (i = 0; i < bufferSize-1; i++) {
        buffer[i] = buffer[i+1]; 
    }

    //set the last item with default values
    strcpy(buffer[bufferSize-1].filename, "X");
    strcpy(buffer[bufferSize-1].source, "X");
    strcpy(buffer[bufferSize-1].destination, "X");
    buffer[bufferSize-1].sourceFd = -5;
    buffer[bufferSize-1].destinationFd = -5;

    currentSize = currentSize -1; //decrement buffer's current size
    return item;
}

//print the buffer
void printBuffer() {
    int i;
    printf("Buffer current size:%d\n", currentSize);
    
    for (i = 0; i < bufferSize; i++) {

        printf("%d - filename:%s \n", i, buffer[i].filename);

        //if we reached the last element
        if(strcmp(buffer[i].filename,"X") == 0){
            return;
        } 
    }
}
