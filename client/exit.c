#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "include/exit.h"
#include "include/settings.h"
#include <sys/socket.h>

#define SLEN 256

void exitm(exit_code e) {
    /*In: exit code
    Log error and exit the program*/
    char buff[SLEN];
    strExit(buff, e); // exit code to error message
    if (e != 0) printf("\n%s\n", buff);
    exit(0);
}

const char EXIT_STRINGS[][SLEN] = {
    "",
    "Permission denied. Are you root?",
    "Failed to connect",
    "Failed to execute the 'walk' script",
    "Failed to read login information",
    "An error occurred while reading or writing meta",
    "Failed to execute the 'resolve' script",
};

/*In: buffer, exit code
Copy into the buffer the matching error message*/
void strExit(char buff[], exit_code e) { snprintf(buff, SLEN+32, "(%i) %s", e, EXIT_STRINGS[e]); }
