#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <sys/time.h>
#include <time.h>

#define MAX_COMMAND 20

//globals
pid_t pids[MAX_COMMAND];
int pipes[MAX_COMMAND][2]; //pipe fds
char *commands[MAX_COMMAND]; //command array
char timeBuffer[80];    //timestamp string
int command_count;  //number of commands


//functions definitions
int checkCommand(char line[]);
int findCommandNum(char line[]);
void childProcess(int idx);
char* createTimestamp();
void signal_handler(int signal);
void freeResources();
void createLogFile();


//signal handler in order to handle if the programs receives any
void signal_handler(int signal){
    if(signal == SIGINT){
        perror("\nSIGINT signal is catched");
    }else if(signal == SIGTERM){
        perror("\nSIGTERM signal is catched");
    }
}


//terminal emulator
int main(int argc, char* argv[]) {
    
    //signal handler operations - SIGINT--------------------------------------------------
    struct sigaction sa;                
    sa.sa_handler = signal_handler;
    sa.sa_flags = 0;
    if(sigemptyset(&sa.sa_mask) == -1){
        perror("sigemptyset");
    }
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("SIGINT\n");
    }
    else if (sigaction(SIGTERM, &sa, NULL) == -1) {
        perror("SIGTERM\n");
    }


    //-------------------------------------------------------------------------------------    

    int pipe_count; //number of | char
    int forkValue;
    int exitTerminal = 0;   //terminating boolean 

    //store original stdin and stdout-----------------------------------------------------
    int stdinFd = dup(STDIN_FILENO);
    if(stdinFd == -1){
        perror("dup - stdin");
        exit(1);
    }
    int stdoutFd = dup(STDOUT_FILENO);
    if(stdoutFd == -1){
        perror("dup - stdoutFd");
        exit(1);
    }

    //terminal------------------------------------------------------------------------------    
    //parent process
    while(!exitTerminal){

        char line[BUFSIZ];  //max command number is 20
        command_count = 0; //init command count

        stdinFd = dup(STDIN_FILENO);
        if(stdinFd < 0) {
            perror("dup - stdin");
            exit(1);
        }
        stdoutFd = dup(STDOUT_FILENO);
        if(stdoutFd < 0) {
            perror("dup - stdout");
            exit(1);
        }

        //get command from user-------------------------------------------------------------
        printf("\nmyShell> ");
        fflush(stdout);
        if(fgets(line, BUFSIZ, stdin) == NULL) {
            perror("fgets");
            for(int d=0; d<MAX_COMMAND; d++){
                commands[d] = "";
            }
            continue;
        }       

        //exit command control--------------------------------------------------------------
        if(strcmp(line,":q\n") == 0){
            exitTerminal = 1;
            continue;
        }

        //invalid command control-----------------------------------------------------------
        if(checkCommand(line) == 0){
            continue;
        }

        //get the number of commands--------------------------------------------------------
        pipe_count = findCommandNum(line);
        command_count = pipe_count + 1;

        if(command_count > 20){
            printf("\n---\nInvalid command number. Maximum command number is 20.\n---\n\n");
            for(int d=0; d<MAX_COMMAND; d++){
                commands[d] = "";
            }
            continue;
        }

        int a=0;
        for(a=0; a<command_count; a++){
            commands[a] = "";
        }

        //split commands--------------------------------------------------------------------
        char *command;
        command = strtok(line  , "|");

        int count=0;
        while(command != NULL && count < command_count){
            commands[count] = command;
            count++;
            command = strtok(NULL, "|");
        }


        //create child processes-----------------------------------------------------------
        int i;
        for(i=0; i<command_count; i++){

            //create pipe----------------------------------------------------------------------
            if (pipe(pipes[i]) == -1) {
                perror("pipe");
                exit(1);
            }
            
            //create child process
            forkValue = fork();

            //failed------------------------------------------------------------------------------
            if(forkValue < 0){
                perror("fork");
                exit(1);
            } 
            //child------------------------------------------------------------------------------
            else if(forkValue == 0){
                
                //execute command in child process
                childProcess(i);

            } 
            //parent-------------------------------------------------------------------------
            else {
                //store child pid
                pids[i] = forkValue;
                //close the pipe write end
                if(close(pipes[i][1])== -1){
                    perror("close pipe[1] - parent");
                    exit(1);
                }
                
                //point the pipe fd to STDIN
                if (dup2(pipes[i][0], STDIN_FILENO) < 0) {
                    perror("dup2 - parent");
                    exit(1);
                }

                // last child
                //print the result to terminal
                if (i == command_count - 1) {                    
                    char buf[BUFSIZ];
                    ssize_t bytes;
                    while ((bytes = read(pipes[i][0], buf, BUFSIZ)) > 0) {
                        write(STDOUT_FILENO, buf, bytes);
                    }
                    
                    if (bytes == -1) {
                        perror("read");
                        exit(1);
                    }
                }

                //close the pipe read end
                if(close(pipes[i][0]) == -1){
                    perror("close pipe[0] - parent");
                    exit(1);
                }   
               
            }
        }

        //create log file for details and free resources(children,fds..etc)
        createLogFile();
        freeResources();

        //point stdin and stdout to currents
        if(dup2(stdinFd, STDIN_FILENO) < 0) {
            perror("dup2 - stdin");
            exit(1);
        }
        if(close(stdinFd) == -1){
            perror("close stdin");
            exit(1);
        }   
        if(dup2(stdoutFd, STDOUT_FILENO) < 0) {
            perror("dup2 - stdout");
            exit(1);
        }
        if(close(stdoutFd)== -1){
            perror("close stdout");
            exit(1);
        } 
    }
    
    return 0;
}



//child process to execute shell command
void childProcess(int idx){
    //close pipe read end
    if(close(pipes[idx][0])== -1){
        perror("close pipe[0] - child");
        exit(1);
    }   

    //point pipe fd to STDOUT
    if (dup2(pipes[idx][1], STDOUT_FILENO)  < 0) {
        perror("dup2 - child");
        exit(1);
    }
    //close pipe write end
    if(close(pipes[idx][1]) == -1){
        perror("close pipe[1] - child");
        exit(1);
    }   

    //execute shell command
    if(execl("/bin/sh", "sh", "-c", commands[idx], NULL) == -1){
        perror("execlp");
        exit(1);
    }           
}


//frees all allocated resources
void freeResources(){
    int j;
    //wait children processes
    for(j = 0; j<command_count; j++){
        wait(NULL);
    }
}

//creates log file with a timestamp
//writes detail of each command per child process
void createLogFile(){
    int logFd;
    char filename[30];

    sprintf(filename, "log_%s.txt", createTimestamp());    //execute shell command
    logFd = open(filename, O_CREAT | O_WRONLY | O_APPEND, 0664);
    if(logFd == -1){
        perror("open logFile");
        exit(1);
    }   

    char log[200];
    for(int c=0; c<command_count; c++){
        int bytesWritten = sprintf(log, "Child%d pid: %d - Command: %s \n", c, pids[c], commands[c]);   
        write(logFd, log, bytesWritten);
    }

    //close log file
    if(close(logFd) == -1){
        perror("close logFile");
        exit(1);
    }   
}



//check validity of the commands
//&&, ;, || commands are not supported
//id the command is not supported return 0, otherwise 1
int checkCommand(char line[]){

    if(strstr(line, "&&") != NULL){
        printf("\n---\nInvalid command '&&'.\nUsage: Enter a shell commands with <, > or |.\n---\n\n");
        return 0;
    }else if(strstr(line, "&") != NULL){
        printf("\n---\nInvalid command '&'.\nUsage: Enter a shell commands with <, > or |.\n---\n\n");
        return 0;
    }else if(strstr(line, ";") != NULL){
        printf("\n---\nInvalid command ';'.\nUsage: Enter a shell commands with <, > or |.\n---\n\n");
        return 0;
    }else if(strstr(line, "||") != NULL){
        printf("\n---\nInvalid command '||'.\nUsage: Enter a shell commands with <, > or |.\n---\n\n");
        return 0;
    }else{
        return 1;
    }
}

//finds occurence of |(pipe) command
//returns the counted amount
int findCommandNum(char line[]){

    int c;
    int count = 0;

    for(c=0; line[c]; c++)  
    {
        if(line[c] == '|' )
        {
          count++;
        }
    }

    return count;
}

//creates a timestamp H:M:S:ms
char* createTimestamp(){

    time_t rawtime;         //holds today raw time value
    struct tm *timeInfo;    //time struct for hour, minute and second
    struct timeval tv;      //tiem struct for miliseconds
    clock_t myTime;

    //get raw time of today
    time(&rawtime);
    timeInfo = localtime(&rawtime);
    //convert to required tiem format (H:M:S)
    strftime(timeBuffer,80,"%H:%M:%S:", timeInfo);
    //get time of today as macroseconds
    gettimeofday(&tv, NULL);
    //convert macroseconds to miliseconds
    unsigned long long millisecondsSinceEpoch = (unsigned long long)(tv.tv_usec) / 1000;
    char ms [50];
    //cponvert long to strşng for miliseconds
    sprintf (ms, "%llu", millisecondsSinceEpoch);
    //combşne strings
    strcat(timeBuffer, ms);
    return timeBuffer;

}
