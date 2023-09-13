#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

//test 
int main(){

    //test values to write to file
    char test1[] = "test1 - create file\n";
    char test2[] = "test2 - dup\n";
    char test3[] = "test3 - dup2\0";

    //create a file-------------------------------------------------------------------
    int fd1 = open("testFilePart2.txt", O_CREAT | O_WRONLY, S_IWUSR | S_IRUSR);
    printf("testFilePart2.txt is created. File descriptor is fd1:%d\n", fd1);
    
    //write test1 to created file-----------------------------------------------------
    if(write(fd1, test1, strlen(test1)) == -1){
        perror("Failed while write() with fd1");
        return 1;
    }
    printf("Written to testFilePart2.txt by fd1:%d\n\n", fd1);


    //create a file descriptor with dup-----------------------------------------------
    int fd2 = dup(fd1);
    printf("File descriptor is created by dup(fd1). New file descriptor value fd2:%d\n", fd2);

    if(write(fd2, test2, strlen(test2)) == -1){
        perror("Failed while write() with fd2");
        return 1;
    }
    printf("Written to testFilePart2.txt by fd2:%d\n\n", fd2);


    //create new file descriptor by dup2----------------------------------------------
    int fd3=dup(fd2);
    printf("File descriptor is created by dup(fd2). New file descriptor value fd3:%d\n", fd3);

    fd3 = dup2(fd3, fd1);
    printf("File descriptor value is changed by dup2(fd3,fd1). New file descriptor value fd3:%d\n", fd3);

    if(write(fd3, test3, strlen(test3)) == -1){
        perror("Failed while write() with fd3");
        return 1;
    }
    printf("Written to testFilePart2.txt by fd3:%d\n\n", fd3);

    //case to fail dup2 (with invalid file descriptor value)--------------------------
    int fd4 = 200;
    fd3 = 200;
    printf("Error test\n");
    fd3 = dup2(fd4, fd3);
    
    //file descriptors are closed-----------------------------------------------------
    close(fd1);
    close(fd2);
    close(fd3);
    return 0;
}




//creates the firstly avalible file descriptor
//returns the new_fd in successful case
//returns -1 in error case
int dup(int old_fd){
    int new_fd;

    //get new firstly avaliable file descriptor
    if((new_fd = fcntl (old_fd, F_DUPFD, 0)) == -1){
        perror("Failed while creating new_fd - dup");
        return -1;
    } else {
        return new_fd;
    }
}




//duplicates the old_fd to new_fd
//returns the new_fd in successful case
//returns -1 in error case
int dup2(int old_fd, int new_fd){

    //special case
    if(old_fd == new_fd){

        //check that old_fd is valid 
        //if it is not set the errno to EBADF
        if(fcntl (old_fd, F_GETFL) == -1){
            perror("Error case - errno is set to EBADF");
            errno = EBADF;
            return -1;
        }
        else {
            return new_fd;
        }
    }

    //check that if the new_fd is already open
    //if it is close it 
    if(fcntl(new_fd, F_GETFL) != -1){
        close(new_fd);
    }

    //get a new_fd, by duplicating the old_fd
    if(fcntl (old_fd, F_DUPFD, new_fd) == -1){
        perror("Failed while duplicating old_fd - dup2");
        return -1;
    } 
    
    return new_fd;

}