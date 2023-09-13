//GONCA EZGI CAKIR 
//151044054
//CSE344 FINAL PROJECT - BibakBOXHelper 

#define DEFAULT_VALUE "X"

//client struct
typedef struct{
    char clientIP[MAX_IP_LENGHT];     //client machine IP
    char dirPath[MAX_DIR_PATH];       //current path hierharchy
    int socketFd;
    pid_t pid;        //client process pid
}
ClientInfo;


//file info struct
typedef struct{
    char name[MAX_NAME_LENGTH];
    char path[MAX_PATH_LENGTH];
    int type;
    off_t size;
    ssize_t bytesRead;
    int modifiedHour;
    int modifiedMinute;
    int modifiedSecond;
} 
FileInfo;

char clientMainDir[MAX_NAME_LENGTH];
char serverMainDir[MAX_NAME_LENGTH];


//initialize server file content array 
void initFileContentArray(FileInfo* fileArray){

    int i;
     for(i=0; i<MAX_FILE; i++){
        strcpy(fileArray[i].name, "");
        strcpy(fileArray[i].name, DEFAULT_VALUE);
        strcpy(fileArray[i].path, "");
        strcpy(fileArray[i].path, DEFAULT_VALUE);
        fileArray[i].type = 0;
        fileArray[i].bytesRead = 0;
        fileArray[i].modifiedHour = 0;
        fileArray[i].modifiedMinute = 0;
        fileArray[i].modifiedSecond = 0;
    }
}
//function finds different files between 2 array of file contents
FileInfo* findDifferences(FileInfo* array1, FileInfo* array2, int size1, int size2) {
    
    //allocate memory for the result array
    FileInfo* result = (FileInfo*)calloc(MAX_FILE, sizeof(FileInfo));
    initFileContentArray(result);

    if (result == NULL) {
        printf("Memory allocation failed.\n");
        return NULL;
    }

    int i, j;
    int count = 0; 

    // iterate over array1 and compare each element with array2
    for (i = 0; i < size1; i++) {
        int found = 0; 

        for (j = 0; j < size2; j++) {
           if ((strcmp(array1[i].name, array2[j].name) == 0 &&
                strcmp(array1[i].path, array2[j].path) == 0 &&
                array1[i].type == array2[j].type &&
                array1[i].bytesRead == array2[j].bytesRead) ||

                (strcmp(array1[i].name, array2[j].name) == 0 &&
                strcmp(array1[i].path, array2[j].path) == 0 &&
                array1[i].type == array2[j].type &&
                array1[i].modifiedHour == array2[j].modifiedHour &&
                array1[i].modifiedMinute == array2[j].modifiedMinute &&
                array1[i].modifiedSecond == array2[j].modifiedSecond)) 
            {
                
                found = 1;
                break;
            }
        }

        // if the element is not found in array2, add it to the result array
        if (!found) {
            result[count] = array1[i];
            count++;
        }
    }

    return result;
}

//function to get the size of the file content array
int getFileContentSize(FileInfo* fileArray){
    
    int fileCount = 0;
   
    for (int i=0; i < MAX_FILE; i++) {
        
        //if the item is file or directory increment the counter
        if(fileArray[i].type == 1 || fileArray[i].type == 2){
            fileCount += 1;
        }
    }

    return fileCount;
}

char* getFileName(char* filePath) {

    //get the last slah
    char* fileName = strrchr(filePath, '/'); 
  
    if (fileName == NULL) {
        fileName = filePath;
    } else {
        fileName++; 
    }

    return fileName;
}

//funtion to get relative path of the file
char* getRelativePath(const char* fullPath, const char* mainDirectory, char* relativePath) {
    
    const char* found = strstr(fullPath, mainDirectory);
    if (found != NULL) {
        size_t mainDirLength = strlen(mainDirectory);
        size_t relativePathLength = strlen(found) - mainDirLength;
        strncpy(relativePath, found + mainDirLength + 1, relativePathLength); // +1 to skip the slash '/'
        relativePath[relativePathLength] = '\0'; // null-terminate the relative path
    }

    return relativePath;
}

//recursive function to traverse directories and gather file information
FileInfo* traverseDirectory(const char* path, FileInfo* fileArray, int* fileCount, int type) {

    DIR* directory;
    struct dirent* entry;
    struct stat fileStat;
    char filePath[MAX_PATH_LENGTH];
    char relativePath[MAX_PATH_LENGTH];

    if ((directory = opendir(path)) == NULL) {
        perror("Unable to open directory");
        return NULL;
    }

    while ((entry = readdir(directory)) != NULL) {
        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0 && strcmp(entry->d_name, "logfile") != 0) {
            snprintf(filePath, sizeof(filePath), "%s/%s", path, entry->d_name);

            if (lstat(filePath, &fileStat) < 0) {
                perror("Unable to get file information.");
                continue;
            }

            FileInfo fileInfo;
            strcpy(fileInfo.name, entry->d_name);
            
            //get the relative path
            if(type==1){
                strcpy(relativePath , getRelativePath(filePath, serverMainDir, relativePath));
            } else if(type == 2){
                strcpy(relativePath , getRelativePath(filePath, clientMainDir, relativePath));
            }

            strncpy(fileInfo.path, relativePath, sizeof(fileInfo.path));
            fileInfo.size = fileStat.st_size;

            time_t modifiedTime = fileStat.st_mtime;
            struct tm* time = localtime(&modifiedTime);
            fileInfo.modifiedHour = time->tm_hour;
            fileInfo.modifiedMinute = time->tm_min;
            fileInfo.modifiedSecond = time->tm_sec;

            //store the read bytes
            //store the file type (1=dir, 2=file)
            if (S_ISREG(fileStat.st_mode)) {
                fileInfo.type = 1;  //file type
                fileInfo.bytesRead = fileStat.st_size;

            } else {
                fileInfo.bytesRead= fileStat.st_size;
                fileInfo.type = 2; //dir type
            }

            // Add file information to the array
            fileArray[*fileCount] = fileInfo;
            (*fileCount)++;

            if (S_ISDIR(fileStat.st_mode)) {
                // Recursively traverse subdirectories
                traverseDirectory(filePath, fileArray, fileCount, type);
            }
        }
    }

    closedir(directory);

    return fileArray;
}


//print server file content array
void printFileContentArray(FileInfo* fileArray){

    int i;
    for(i=0; i<MAX_FILE; i++){
        if(strcmp(fileArray[i].name , DEFAULT_VALUE) != 0){

            printf("name:%s | path:%s | type:%d | size:%ld | bytes:%zd | time:%d:%d:%d\n", 
            fileArray[i].name,
            fileArray[i].path,
            fileArray[i].type,
            fileArray[i].size, 
            fileArray[i].bytesRead, 
            fileArray[i].modifiedHour,
            fileArray[i].modifiedMinute,
            fileArray[i].modifiedSecond);
        }
    }
}


