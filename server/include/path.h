/*Header for server/path.c file*/
void createFile(char path[], char data[]);
size_t fileSize(char path[]);
void readFile(char buff[], char path[], char mode[]);
int isDir(const char *path);
int isFile (char *fname);
