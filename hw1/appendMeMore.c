#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

//test
int main(int argc, char* argv[]) {

    int is_append = 1;  //boolean for append flag usage
    int fd;             //file descriptor


    //check that is there enough console arguments-------------------------------
    if(argc < 3){
        perror("Usage: Console argument can't be less than 3.");
        return 1;
    }
    else if(argc > 4){
        perror("Usage: Console argument can't be more than 4.");
        return 1;
    }
    //checks that is there argument x for lseek usage
    else if(argc == 4){
        if(strcmp(argv[3], "x") == 0){
            //use lseek to add bytes to file
            is_append = 0;
        } else {
            perror("Usage: Last console argument is invalid, try 'x'.");
            return 1;
        }
    }

    //store arguments------------------------------------------------------------
    const char *filename = argv[1];
    const int num_bytes = atoi(argv[2]);

    //open or create the file without append flag--------------------------------
    if(is_append){
        fd = open(filename,  O_WRONLY | O_CREAT | O_APPEND, 0644);
    } 
    //open or create the file without append flag
    else {
        fd = open(filename,  O_WRONLY | O_CREAT , 0644);
    }

    if(fd == -1){
        perror("Failed while open()");
        return 1;
    }

    //write bytes into the file---------------------------------------------------
    char buffer[1] = {'a'};
    for(int i=0 ; i < num_bytes ; i++){

        //if there is an x argument---------------------------------------------------
        //use lseek to move to end of the file
        if(!is_append){
            if(lseek(fd, 0, SEEK_END) == -1){
                perror("Failed while lseek()");
                return 1;
            }
        }

        if(write(fd, buffer, 1) == -1){
            perror("Failed while write()");
            return 1;
        }
    }

    //close the file
    if(close(fd) == -1){
        perror("Failed while close()");
        return 1;
    }
    
    return 0;
}
