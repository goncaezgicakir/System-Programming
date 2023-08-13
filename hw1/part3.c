#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>

int main(int argc, char* argv[]) {

    int fd;
    int dup_fd;
    off_t offset_fd;
    off_t offset_dup_fd;
    struct stat inode_fd;
    struct stat inode_dup_fd;

    //create a file and write sth--------------------------------------------
    fd = open("testFilePart3.txt", O_CREAT | O_WRONLY, S_IWUSR | S_IRUSR);
    if(write(fd, "This is a test value.", 21) == -1){
        perror("Failed while write() with fd");
        return 1;
    }
    printf("testFilePart3.txt is created. Written by using file descriptor fd:%d\n\n", fd);

    //create afile descriptor by dup----------------------------------------
    dup_fd = dup(fd);
    printf("New file descriptor is created by dup(fd). File descriptor value dup_fd:%d\n\n", dup_fd);


    //get the file offsets for each file descriptor-------------------------
    offset_fd = lseek(fd, 0, SEEK_CUR);
    if(offset_fd  == -1){
        perror("Failed while lseek() of offset_fd");
        return 1;
    }
    offset_dup_fd = lseek(dup_fd, 0, SEEK_CUR);
    if(offset_dup_fd == -1){
        perror("Failed while lseek() of offset_dup_fd");
        return 1;
    }

    //get the file inodes for each file descriptor--------------------------
    if(fstat(fd, &inode_fd) == -1){
        perror("Failed while fstat() of fd");
        return 1;
    }

    if(fstat(dup_fd, &inode_dup_fd) == -1){
        perror("Failed while fstat() of dup_fd");
        return 1;
    }

    //print results---------------------------------------------------------
    printf("File descriptors offset and inode values to proof that they point to same file and offset.\n");
    printf("offset fd -> offset:%ld  fd inode:%ld\n", offset_fd, inode_fd.st_ino);
    printf("offset dup_fd -> offset:%ld  dup_fd inode:%ld\n", offset_dup_fd, inode_dup_fd.st_ino);

    //close the files-------------------------------------------------------
    if(close(fd) == -1){
        perror("Failed while close() of fd");
        return 1;
    }
    if(close(dup_fd) == -1){
        perror("Failed while close() of dup_fd");
        return 1;
    }

    return 0;
}

