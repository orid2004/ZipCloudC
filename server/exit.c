#include <stdio.h>
#include <stdlib.h>
#include "include/exit.h"
#include "include/settings.h"
#include <sys/socket.h>

#define SLEN 256

void exitm(exit_code e) {
    /*In: exit code
    Log error and exit the program*/
    char buff[SLEN+32];
    strExit(buff, e);
    printf("\n%s\n", buff);
    exit(0);
}

const char EXIT_STRINGS[][SLEN] = {
    "", //0
    "Permission denied. Are you root?", //1
    "Failed to bind", //2
    "Failed to listen", //3
    "Aborted due to SSL error", //4
    "An error occurred while reading encryption keys", //5
    "Configuration failure", //6
    "An error occured while reading meta", //7
    "Failed to execute the 'walk' script" //8
};

/*In: buffer, exit code
Copy into the buffer the matching error message*/
void strExit(char buff[], exit_code e) { snprintf(buff, SLEN+32, "(%i) %s", e, EXIT_STRINGS[e]); }
