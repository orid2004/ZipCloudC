// libraries
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <malloc.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/types.h>
#include <resolv.h>
#include <sys/io.h>
#include <sys/mman.h>
#include <netdb.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include "include/connection.h"
#include "include/settings.h"
#include "include/exit.h"
#include "include/path.h"

#define RED "\033[31m"
#define RESET "\033[0m"
#ifndef O_LARGEFILE
#define O_LARGEFILE 0
#endif

// Functions
void createDirs(const char names[]);
int splitv(char buff[], const char str[], const char delim[], int index);
void logProgress(size_t i);
unsigned long hash64(const unsigned char *str);
int loadId(const char id[]);
void saveId();
void deleteId();
void absPath(char buff[], int len, const char input[]);
void pathToDirName(char buff[], const char path[]);
int authenticate(SSL *ssl, const char id[]);
size_t sendChunk(SSL *ssl, char buff[], size_t len);
void sendDir(SSL *ssl, char path[], const char id[]);
void downloadDir(SSL *ssl, const char name[], const char outdir[], const char id[]);
int hasDir(SSL *ssl, char name[], const char id[]);
int handleUploadRequest(SSL *ssl, char *argv[], const char id[]);
void getUploadResponse(SSL *ssl, char buff[], size_t size);
void tree(SSL *ssl, const char id[]);
void handleUserRequest(SSL *ssl, int argc, char *argv[], const const char id[]);
void sendReq(SSL* ssl, const char req[], int n_len);
int getData(SSL *ssl, char buff[], int len);

int main(int argc, char *argv[]) {
    // Main function
    char id[256];
    createDirs(MKDIR_STR);
    if (getuid() != 0) exitm(NOT_ROOT); // require root user
    else if (argc == 2 && strcmp(argv[1], "auth") == 0) saveId(); // collect and save login information
    else if (argc == 2 && strcmp(argv[1], "deauth") == 0) deleteId(); // delete last saved login information
    else if (argc < 2 || argc > 4) printf("Usage: %s <option> <optional values>\n", argv[0]);
    else {
        SSL_library_init(); // init ssl lib
        SSL_CTX *ctx = InitClientCTX(); // create ssl context
        int d = bindAddr(LOCALHOST, PORT); // bind address
        SSL *ssl = SSL_new(ctx); // hold data for the SSL cocnnection
        SSL_set_fd(ssl, d); // assigns a socket to a SSL structure
        if (SSL_connect(ssl) == FAIL) exitm(CONNECT_ERR); // connect to server
        if (loadId(id) != FAIL) {
            handleUserRequest(ssl, argc, argv, id); // upload folder
            close(d); // close socket descriptor
            SSL_free(ssl); // close ssl
            SSL_CTX_free(ctx); // release context
        }
    }
    exitm(OK);
}

void createDirs(const char names[]) {
    /* In: directories splited by a comma
    Create directoris */
    int i = 0;
    char dir[1024];
    struct stat st = {0};
    while (splitv(dir, names, ",", i++) == 0)
        if (stat(dir, &st) == -1) mkdir(dir, 0777); // create folder
}

int splitv(char buff[], const char str[], const char delim[], int index) {
    /* In: a buffer, string, delimeter and object index
    Split a string. Copies the index-th element into a buffer
    Return a non 0 value when fail*/
    char *start = (char*)str, *end = strstr(str, delim);
    int i = 0, buff_i = 0, copy_len;
    for (i = 0; i < index && end != NULL; i++) {
        // Search for the next token
        start = end + strlen(delim);
        end = strstr(start, delim);
    }
    if (i != index) return index; // index error
    if (end == NULL) copy_len = strlen(start); // private case: index is last
    else copy_len = (long)(end - start); // token length
    memcpy(buff, start,copy_len); // copy token to buffer
    buff[copy_len] = '\0'; // terminate the buffer
    return 0;
}

void logProgress(size_t i) {
    /*In: percentage
    Print progress*/
    printf("\r%li%%", i);
    fflush(stdout);
}

unsigned long hash64(const unsigned char *str) {
    /*In: a string
    Return a hash*/
    unsigned long hash = 5381;
    int c;
    while (c = *str++)
        hash = ((hash << 5) + hash) + c;
    return abs((int64_t)hash); // handle negative values
}

int loadId(const char id[]) {
    /*In: id buffer
    Read the login information created by 'auth'*/
    char id_path[1024];
    snprintf(id_path, sizeof(id_path), "%s/%s", ID_DIR, ID_FNAME); // set id file path
    FILE *f = fopen(id_path, "r");
    if (f == NULL) printf("Found No login information.\nRun the 'auth' option to set credentials and try again...\n");
    else {
        fread((char*)id, 1, fileSize(id_path), f); // copy to buffer
        fclose(f); // close the file
        return 0;
    }
    return -1;
}

void saveId() {
    /*Implementation of the 'auth' option
    save new login information*/
    char id[256], id_path[1024], tmp[128];
    snprintf(id_path, sizeof(id_path), "%s/%s", ID_DIR, ID_FNAME); // set id file path
    printf("Save login information locally:\nUsername: ");
    scanf("%s", id); getchar(); // collect a username
    printf("Password: ");
    scanf("%s", tmp); getchar(); // collect a password
    // Form an id structure
    strcat(id, "\r\n");
    strcat(id, tmp);
    FILE *f = fopen(id_path, "w"); // open id file
    if (f == NULL) printf("Failed to saved credentials under %s\n", id_path);
    else {
        fwrite(id, 1, strlen(id), f); // save the credentials
        fclose(f); // close the file
        // log
        printf("\nCredentials are saved locally and will serve your requests.\n");
        printf("Use the 'auth' option to overwrite the credentials.\nUse the 'deauth' option to delete the credentials.\n");
    }
}

void deleteId() {
    /*Implementation of the 'deauth' option
    Delete last login information*/
    char id[256], id_path[1024], uname[128], tmp[128];
    snprintf(id_path, sizeof(id_path), "%s/%s", ID_DIR, ID_FNAME); // set id file path
    if (!isFile(id_path)) printf("There are no credentials to delete...\n");
    else {
        // Read a username
        FILE *f = fopen(id_path, "r"); // open id file
        fread(id, 1, fileSize(id_path), f); // read into buffer
        fclose(f); // close the file
        // Get the user confirmation
        splitv(uname, id, "\r\n", 0);
        printf("Request to delete the credentials of {%s}. Continue? [y/n]: ", uname);
        scanf("%s", tmp); getchar();
        if (strcmp(tmp, "y") == 0) remove(id_path); // remove id path
        else printf("Aborted\n"); // abort
    }
}

void absPath(char buff[], int len, const char input[]) {
    /*In: a buferr, its size and a path
    Copy the absolute path of the givven path*/
    char cwd[1024]; // cwd buffer
    getcwd(cwd, sizeof(cwd)); // copy the current working directory
    snprintf(buff, len, "%s/%s", cwd, input);
}

void pathToDirName(char buff[], const char path[]) {
    /*In: a buffer, absolute path of a directory
    Copy the directory name extracted from the path*/
    char delim[] = "/", tmp[1024];
    strcpy(tmp, path);
    char *ptr = strtok(tmp, delim); // split next
	while(ptr != NULL) {
        strcpy(buff, ptr); // copy current directory to buff
		ptr = strtok(NULL, delim); // split next
	}
    // The last token was copied into buff
}

int authenticate(SSL *ssl, const char id[]) {
    /*In: ssl, id struct
    Send an authenticate request. Return <0 for access*/
    char auth_req[512], response[2];
    snprintf(auth_req, sizeof(auth_req), "%s\r\n%s", AUTH_REQ, id); // set an auth request
    sendReq(ssl, auth_req, 0); // send an auth request
    getData(ssl, response, sizeof(response)); // wait for a response
    return strcmp(response, ACCESS_OK) == 0; // check for ACCESS_OK
}

void sendDir(SSL *ssl, char path[], const char id[]) {
    /*In: ssl, meta path, id struct
    Send a directory to the servers*/
    const char out_file[128] = "/tmp/out.meta";
    char dir_req[512], file_path[1024];
    // Execute the walk script to group meta (files, folders) and content.
    if (fork() == 0) { if (execl(WALK, WALK, "--walk", path, NULL) == FAIL) exitm(WALK_FAIL); }
    else wait(NULL);
    size_t meta_len = fileSize(out_file), bytes_read, bytes_sent, total_bytes_sent = 0, chunk_stat = 0;
    off_t chunk = 0;
    // Send a dir request followed by id and meta size (in bytes).
    snprintf(dir_req, sizeof(dir_req), "%s\r\n%s", UPLOAD_REQ, id); // set a dir request
    sendReq(ssl, dir_req, meta_len); // send request
    // Read and send chunks from the file
    unsigned char *buff = malloc(meta_len);
    int f = open(out_file, O_RDONLY|O_LARGEFILE); // open walk output
    if (f == -1) exitm(META_ERR);
    while (chunk < meta_len) {
        bytes_read = read(f, ((char *)buff + chunk_stat), FILE_CHUNK); // read a chunk
        chunk_stat += bytes_read; // update sock_chunk stat
        chunk += bytes_read; // update bytes read
        if (bytes_read < 0 ) {
            // exit the program on failure
            free(buff);
            close(f);
            exitm(META_ERR);
        }
        if (chunk_stat >= SOCK_CHUNK - FILE_CHUNK) {
            /*Socket chunk is full.
            SOCK_CHUNK bytes are ready to be sent*/
            bytes_sent = SSL_write(ssl, (char*)buff, chunk_stat); // send to server
            total_bytes_sent += bytes_sent; // update total bytes sent
            chunk_stat -= bytes_sent; // update chunk stat
            logProgress((int)(((double)total_bytes_sent / meta_len)*100)); // log progress
        }
    }
    if (chunk_stat > 0) {
        // Private case: Loop is done reading but hasn't sent the last chunk
        total_bytes_sent += SSL_write(ssl, (char*)buff, chunk_stat); // update total_bytes
        logProgress((int)(((double)total_bytes_sent / meta_len)*100)); // log 100%
    }
    printf("\n");
    if (total_bytes_sent < meta_len)
        // Private case: Server didn't recieved the entire meta file
        printf("Failed to upload %li KB. Folder is damaged. Try again...\n", (size_t)((meta_len - total_bytes_sent)/1000));
    free(buff); // free buffer
    close(f); // close the file
    remove(out_file); // delete walk output
}

void downloadDir(SSL *ssl, const char name[], const char outdir[], const char id[]) {
    /*In: ssl, folder name, out directory, id struct
    Download 'name' folder and save to 'outdir'*/
    char req[2048], buff[1024];
    snprintf(req, sizeof(req), "%s\r\n%s\r\n%s", DOWNLOAD_REQ, id, name); // set a download request stirng
    sendReq(ssl, req, 0); // send a download request
    getData(ssl, buff, sizeof(buff)); // wait for meta len from server
    size_t meta_len = atoi(buff), total_bytes = 0, bytes_write, bytes_read;
    if (meta_len < 0) printf("%-20s Authentication failed... Valid your information.\n%-20s Try a different unique username...\n", "Registered Users", "New Users");
    else if (meta_len == 0) printf("Folder not found. Use the 'tree' command to list your account\n");
    else {
        printf("Downloading...\n");
        char meta_path[1024];
        snprintf(meta_path, sizeof(meta_path), "/tmp/%ld.meta", hash64(id)); // temp download path
        unsigned char *meta_buff = malloc(SOCK_CHUNK + 1024);
        FILE *meta = fopen(meta_path, "wb"); // create a temp meta file
        if (meta == NULL) exitm(META_ERR);
        SSL_write(ssl, LISTENING, 2); // send a listening sign
        while ((bytes_read = SSL_read(ssl, meta_buff, SOCK_CHUNK)) > 0) {
            // Read chunks from server
            bytes_write = fwrite(meta_buff, 1, bytes_read, meta); // update meta file
            total_bytes += bytes_write; // update total bytes written
            if (total_bytes >= meta_len) break;
        }
        /*Execute walk script with decompress flag
         Create the output directory under 'outdir'*/
        if (fork() == 0) { if (execl(WALK, WALK, "-d", meta_path, outdir, NULL) == FAIL) exitm(WALK_FAIL); }
        else wait(NULL);
        remove(meta_path); // remove temp meta file
    }
}

int hasDir(SSL *ssl, char name[], const char id[]) {
    /*In: ssl, folder name, id struct
    Check if account has a 'name' folder*/
    char dir_req[1024], buff[1000], response[2];
    snprintf(dir_req, sizeof(dir_req), "%s\r\n%s\r\n%s", ISDIR_REQ, id, name); // set an is-dir request
    sendReq(ssl, dir_req, 0); // send an is-dir request
    getData(ssl, response, sizeof(response)); // wait for a response
    return strcmp(response, ACCESS_OK) == 0;
}

int handleUploadRequest(SSL *ssl, char *argv[], const char id[]) {
    /*In: ssl, arg vector, id struct
    Handle the input of an upload request*/
    char path[1024], dir_name[256];
    if (argv[2][0] != '/') absPath(path, sizeof(path), argv[2]); // convert to absolute path
    else strcpy(path, argv[2]);
    if (!isDir(path)) {
        printf("Folder not found\n"); // invalid path
        return -1;
    }
    pathToDirName(dir_name, path); // get the folder name
    // Check if a folder named the same is already exsists under the accound
    if (hasDir(ssl, dir_name, id)) {
        printf("Folder named %s exists... Use the 'tree <folder>' option\n", dir_name);
        return -1;
    }
    sendDir(ssl, path, id); // send the directory
    return 0;
}

void getUploadResponse(SSL *ssl, char buff[], size_t size) {
    /*In: ssl, buffer, buffer size
    Wait for a responses after sending a directory*/
    char response[2];
    getData(ssl, response, sizeof(response)); // wait for data
    if (strcmp(response, ACCESS_OK) == 0) strcpy(buff, "Folder was uploaded successfully.\nServer is processing the request...");
    else if (strcmp(response, ACCESS_WP) == 0) snprintf(buff, size, "%-20s Authentication failed... Valid your information.\n%-20s Try a different unique username...\n", "Registered Users", "New Users");
    else if (strcmp(response, NEW_USER) == 0) strcpy(buff, "Welcome New user! Folder was uploaded successfully.");
    else if (strcmp(response, FOLDER_EXISTS) == 0) strcpy(buff, "Folder exists... Use the 'tree <folder>' option.");
    else strcpy(buff, "Unknown response from server\n");
}

void tree(SSL *ssl, const char id[]) {
    /*In: ssl, arg vector, arg count
    List account*/
    char tree_req[1024], len_buff[512], uname[128];
    splitv(uname, id, "\r\n", 0);
    snprintf(tree_req, sizeof(tree_req), "%s\r\n%s", TREE_A_REQ, id); // set a tree account request
    sendReq(ssl, tree_req, 0); // send a tree request
    getData(ssl, len_buff, sizeof(len_buff)); // wait for length
    size_t len = atoi(len_buff), bytes_read, total_bytes = 0;
    if (len < 0) printf("%-20s Authentication failed... Valid your information.\n%-20s Try a different unique username...\n", "Registered Users", "New Users");
    else if (len == 0) printf("Account not found. Use the 'upload <folder>' option.\n");
    else {
        char buff[SOCK_CHUNK]; // output buffer
        SSL_write(ssl, LISTENING, 2); // send a listening sign
        SSL_read(ssl, buff, len); // read the tree output
        if (atoi(buff) == -1) printf("%s\n|___\n0 Folders\n", uname); // Handle empty account
        else printf("%s\n", buff); // Tree output
    }
}

void rmDir(SSL *ssl, char name[], const char id[]) {
    /*In: ssl, folder name, id struct
    Delete a directory from account*/
    char req[2048], tmp[4];
    if (strcmp(name, "_") == 0) {
        printf("Request to delete the following folder(s):\n" RED);
        tree(ssl, id); // warn about the deleted folders
        printf(RESET "Continue? [y/n]: "); // get user confirmation
    }
    else printf("Request to delete %s%s%s. Continue? [y/n]: ", RED, name, RESET);
    // Wait for the user confirmation
    scanf("%s", tmp); getchar();
    if (strcmp(tmp, "y") == 0) {
        if (name[strlen(name) - 1] == '/') name[strlen(name) - 1] = '\0'; // Handle backslash
        snprintf(req, sizeof(req), "%s\r\n%s\r\n%s", DELETE_REQ, id, name); // set a delete request
        sendReq(ssl, req, 0); // send a delete request
    }
    else printf("Aborted\n");
}

void handleUserRequest(SSL *ssl, int argc, char *argv[], const char id[]) {
    /*In: ssl, argc, arg vector, id struct
    Handle the input and process request*/
    char auth_req[512], option[128], response[512];
    strcpy(option, argv[1]);
    // Authenticate before sending request to prevent long waiting in case of invalid login information
    if (!authenticate(ssl, id)) printf("%-20s Authentication failed... Valid your information.\n%-20s Try a different unique username...\n", "Registered Users", "New Users");
    else {
        if (strcmp(option, "upload") == 0 || strcmp(option, "-u") == 0) {
            // Handle the upload request input
            if (argc != 3) printf("Usage: %s upload <folder>\n", argv[0]);
            else if (handleUploadRequest(ssl, argv, id) != FAIL) {
                getUploadResponse(ssl, response, sizeof(response));
                printf("%s\n", response);
            }
        }
        else if ((strcmp(option, "download") == 0 || strcmp(option, "-d") == 0)) {
            /* Handle the download request input
            Output directory is optional (default is cwd)*/
            if (argc != 3 && argc != 4) printf("Usage: %s download <folder> <outdir/optional>\n", argv[0]);
            else if (argc == 4) downloadDir(ssl, argv[2], argv[3], id);
            else downloadDir(ssl, argv[2], ".", id);
        }
        else if (strcmp(option, "tree") == 0 || strcmp(option, "-t") == 0) {
            // Handle the tree request input
            if (argc == 2) tree(ssl, id);
            else printf("Usage:\nList Account: %s tree\nList folder: %s tree <folder>\n", argv[0], argv[0]);
        }
        else if (strcmp(option, "rm") == 0 || strcmp(option, "-r") == 0) {
            // Handle remove request
            if (argc == 3) rmDir(ssl, argv[2], id);
            else  printf("Usage: %s remove <folder>\n", argv[0]);
        }
        else printf("Usage: %s <option> <optional values>\n", argv[0]); // Unkown option
    }
}

void sendReq(SSL* ssl, const char req[], int n_len) {
    /*In: ssl, request, len
    Send a reques
    n_len: If a request is followed up with additional information 'nlen' holds its length.
    Otherwise, n_len is meaningless*/
    char buff[strlen(req) + 128];
    snprintf(buff, sizeof(buff), "%s\r\n%i", req, n_len); // set a request string
    SSL_write(ssl, buff, strlen(buff)); // send the request
}

int getData(SSL *ssl, char buff[], int len) {
    /*In: ssl, buffer, buffer size
    Wait to data and copy to a buffer*/
    if (buff == NULL) return -1;
    int bytes = SSL_read(ssl, buff, len); // wait for data
    buff[bytes] = '\0'; // set the null terminator
    return bytes;
}
