#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <string.h>

void createFile(char path[], char data[]) {
    /*In: file path, data string, mode
    Create a file and write data*/
    FILE *f = fopen(path, "wb");
    fwrite(data, strlen(data), 1, f);
    fclose(f);
}

size_t fileSize(char path[]) {
    /*In: file path
    Return file size in bytes*/
    struct stat st;
    if(stat(path, &st) != 0) return 0;
    return st.st_size;
}

void readFile(char buff[], char path[], char mode[]) {
    /*In: a buffer, file path, mode
    Read file content and copy to buffer*/
    long fsize = fileSize(path); // get file size
    FILE *f = fopen(path, mode); // open the file
    fread(buff, 1, fsize, f); // read content
    buff[fsize] = '\0'; // copy to buffer
    fclose(f);
}

int isDir(const char *path) {
    /*In: dir path
    Check if a folder exists*/
    DIR* dir = opendir(path);
    if (dir) {
        closedir(dir);
        return 1;
    }
    return 0;
}

int isFile (char *fname) {
    /*In: file path
    Check if a file exists*/
    struct stat buffer;
    return (stat (fname, &buffer) == 0);
}
