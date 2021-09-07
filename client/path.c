#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <string.h>
#include "include/path.h"

size_t fileSize(const char path[]) {
    /*In: file path
    Return file size in bytes*/
    struct stat st;
    if(stat(path, &st) != 0) return 0;
    return st.st_size;
}

int isDir(const char path[]) {
    /*In: dir path
    Check if a folder exists*/
    DIR* dir = opendir(path);
    if (dir) {
        closedir(dir);
        return 1;
    }
    return 0;
}

int isFile(const char path[]) {
    /*In: file path
    Check if a file exists*/
    struct stat buffer;
    return (stat (path, &buffer) == 0);
}
