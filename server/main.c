// libraries
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <malloc.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <dirent.h>
#include <resolv.h>
#include <stdint.h>
#include <fcntl.h>
#include "openssl/ssl.h"
#include "openssl/err.h"
#include "include/exit.h"
#include "include/settings.h"
#include "include/connection.h"
#include "include/path.h"

#define FAIL -1
#ifndef O_LARGEFILE
#define O_LARGEFILE 0
#endif

// globals
char server_prefix[1024], server_cert[1024];

// SSL client
typedef struct Client {
    SSL *ssl;
    int d;
    struct sockaddr_in addr;
    socklen_t len;
} Client;

// functions
void createDirs(const char names[]);
void strremove(char buff[], char str[], const char sub[]);
int endsWith (const char str[], const char end[]);
int splitv(char buff[], const char str[], const char delim[], int index);
void logProgress(size_t i);
unsigned long hash64(const unsigned char *str);
void readConf(const char conf[]);
int validConf();
void acceptConnections(int server, SSL_CTX *ctx);
void initFolder(Client *client, const char meta_path[], const char id[]);
void saveMeta(Client *client, const char name[], const char root_path[], const char meta_path[], int id_stat);
size_t sendChunk(SSL *ssl, char buff[], size_t len);
void sendMeta(Client *client, const char dir_name[], const char id[]);
void sendDirStat(Client *client, const char dir_name[], const char id[]);
int idStat(const char id[]);
void handleUploadRequest(Client *client, const char id[], size_t meta_len);
void handleAuthRequest(Client *client, const char id[], const char addr_st[]);
void handleTreeRequest(Client *client, const char id[]);
void rmDir(Client *client, const char name[], const char id[]);
void handleClient(int server, Client *client, SSL_CTX *ctx);

int main(int argc, char *argv[]){
    // setup & mainloop
    createDirs(MKDIR_STR); // create directories
    if (getuid() != 0) exitm(NOT_ROOT); // require root
    else if (argc != 2) printf("Usage: %s <conf>\n", argv[0]); // no configuration file/ too many args
    else {
        readConf(argv[1]); // setup from configuration file
        if (!validConf()) exitm(CONFIG_ERR); // missing prefixs
        SSL_library_init(); // init ssl lib
        SSL_CTX *ctx = InitServerCTX(); // create ssl context
        useCertificate(ctx, server_cert, server_prefix); // apply certificates and setup context
        int server = bindAddr(PORT); // bind address to socket
        acceptConnections(server, ctx); // accept connection from clients
    }
    exitm(OK);
}

void createDirs(const char names[]) {
    /* In: directories splited by a comma
    Create directories */
    int i = 0;
    char dir[1024];
    struct stat st = {0};
    while (splitv(dir, names, ",", i++) == 0)
        if (stat(dir, &st) == -1) mkdir(dir, 0777); // create directory
}

void strremove(char buff[], char str[], const char sub[]) {
    /*In: buffer, string, sub-string
    Remove the sub-string from the string*/
    char *p, *q, *r;
    if ((q = r = strstr(str, sub)) != NULL) {
        size_t len = strlen(sub);
        while ((r = strstr(p = r + len, sub)) != NULL)
            while (p < r) *q++ = *p++;
        while ((*q++ = *p++) != '\0') continue;
    }
    strcpy(buff, str); // copy to buffer
}

int endsWith (const char str[], const char end[]) {
    /*In: a string, sub-string
    Check if a string ends with a sub-string*/
    size_t slen = strlen (str);
    size_t elen = strlen (end);
    if (slen < elen) return 0;
    return (strcmp (&(str[slen-elen]), end) == 0);
}

int splitv(char buff[], const char str[], const char delim[], int index) {
    /* In: a buffer, string, delimeter and object index
    Split a string. Copy the index-th element into a buffer.
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
        hash = ((hash << 5) + hash) + c; // hash
    if ((int64_t)hash < 0) return (int64_t)hash * (-1); // Handle negative hash
    return (int64_t)hash; // convert to int64
}

void readConf(const char conf[]) {
    /*In: path
    Read the configuration file
    Search for a server-certificate and a server-prefix
    Format:
    prefix=value*/
    char buff[1024], rmvstr[3][16] = {" ", "\n", 13}, prefix[32], value[1024];
    int rmvstrsize = 3; // sub-strings from configuration line to be removed
    FILE *f = fopen(conf, "r"); // open configuration file
    if (f == NULL) {
        printf("Failed to read configuration file {%s}\n", conf); // log error
        exitm(CONFIG_ERR);
    }
    while (fgets(buff, sizeof(buff), f) != NULL) {
        // Read line from configuration file
        for (int i = 0; i < rmvstrsize; i++) strremove(buff, buff, rmvstr[i]); // Remove unnecessary sub-strings
        splitv(prefix, buff, "=", 0); // extract a prefix
        splitv(value, buff, "=", 1); // extract a value
        if (strcmp(prefix, KEY_PREFIX) == 0) strcpy(server_prefix, value); // search for a server prefix
        else if (strcmp(prefix, CRT_PREFIX) == 0) strcpy(server_cert, value); // seatch for a server certificate
    }
}

/*Check if prefix and certificate buffers have value*/
int validConf() { return strlen(server_prefix) > 0 && strlen(server_cert) > 0; }

void acceptConnections(int server, SSL_CTX *ctx) {
    /*In: server descriptor, context
    Accept connections from client
    Create a child process for each client*/
    Client *client;
    printf("Waiting for clients to connect on port %i...\n\n", PORT);
    while (1) {
        client = malloc(sizeof(Client)); // allocate memory for a client
        client->len = sizeof(client->addr); // config addr length
        client->d = accept(server, (struct sockaddr*)&(client->addr), &(client->len)); // accept a client
        client->ssl = SSL_new(ctx); // hold data for the SSL cocnnection
        SSL_set_fd(client->ssl, client->d); // assigns a socket to a SSL structure
        if (SSL_accept(client->ssl) != FAIL)
            // Handle the client's request frms a child process
            if (!fork()) handleClient(server, client, ctx);
    }
}

void initFolder(Client *client, const char meta_path[], const char id[]) {
    /*In: a client, meta path, id struct
    Register a new user if necessary and process the request*/
    char dir_name[1024], uname[128], root_path[512], id_path[1024], hash_id[64];
    FILE *f = fopen(meta_path, "r"); // open meta file
    fread(dir_name, 1, sizeof(dir_name), f); // read the directory name
    fclose(f); // close the file
    *strstr(dir_name, "\r\n\r\n") = '\0'; // remove unnecessary data from dir name
    if (splitv(uname, id, "\r\n", 0) != 0) SSL_write(client->ssl, ACCESS_WP, 2); // id struct
    else {
        snprintf(root_path, sizeof(root_path), "%s/%lu", APP_DATA, hash64(uname)); // root directory path
        snprintf(id_path, sizeof(id_path), "%s/__id__", root_path); // id file path
        snprintf(hash_id, sizeof(hash_id), "%lu", hash64(id)); // 64 hash generated by id
        int id_stat = idStat(id); // get id stat
        if (id_stat == 0) {
            // New user
            createDirs(root_path); // create a root path
            createFile(id_path, hash_id); // create an id file holding the hash
            printf("{%lu} Registerd new user.\n", hash64(uname));
        }
        if (id_stat >= 0) {
            // Registerd and new users
            printf("{%lu} Processing %s... ", hash64(uname), dir_name);
            saveMeta(client, dir_name, root_path, meta_path, id_stat); // process the request
            printf("Done!\n");
        }
        else {
            /*Wrong password
            Attemp to access a Registerd user with a wrong password*/
            printf("{%lu} Failed login attemp\n", hash64(uname));
            SSL_write(client->ssl, ACCESS_WP, 2); // send a response
        }
    }
}

void saveMeta(Client *client, const char name[], const char root_path[], const char meta_path[], int id_stat) {
    /*In: client, dir name, root path, meta path, id stat
    Save the meta if directory not exists
    Send an appropriate response*/
    char file_in_root[strlen(root_path) + 512];
    snprintf(file_in_root, sizeof(file_in_root), "%s/%s.zip", root_path, name); // destination zip file
    if (isFile(file_in_root)) SSL_write(client->ssl, FOLDER_EXISTS, 2); // check if directory already exists
    else {
        if (id_stat == 0) SSL_write(client->ssl, NEW_USER, 2); // send a response to a new user
        else SSL_write(client->ssl, ACCESS_OK, 2); // send a reponse to a registerd user
        if (fork() == 0) { if (execl(WALK, WALK, "-c", meta_path, root_path, name, NULL) == FAIL) exitm(WALK_FAIL); } // compress the meta file
        else (wait(NULL));
    }
}

void sendMeta(Client *client, const char dir_name[], const char id[]) {
    /*In: a client, dir name to download, id struct
    Send a meta file*/
    if (idStat(id) < 0) {
        /*Attemp to download with a wrong password
        Send a negative response*/
        SSL_write(client->ssl, "-1", 2);
        return;
    }
    char uname[128],  meta_path[1024], decomp_path[1024], tmp[128];
    splitv(uname, id, "\r\n", 0); // get the username
    snprintf(meta_path, sizeof(meta_path), "%s/%lu/%s.zip", APP_DATA, hash64(uname), dir_name); // meta path to download
    if (!isFile(meta_path)) {
        /*Attempt to download folder that not exists
        Send a zero response*/
        SSL_write(client->ssl, "0", 1);
        return;
    }
    if (fork() == 0) { if (execl(WALK, WALK, "-d", meta_path, "/var/lib/zecure", NULL) == FAIL) exitm(WALK_FAIL); } // decompress the meta
    else (wait(NULL));
    snprintf(decomp_path, sizeof(decomp_path), "/var/lib/zecure/tmp/%ld.meta", hash64(id)); // decompressed meta path

    /*Send a meta length
    Wait for a listening sign*/
    size_t meta_len = fileSize(decomp_path), bytes_read, total_sent = 0, chunk_stat = 0;
    off_t chunk = 0;
    unsigned char *buff = malloc(SOCK_CHUNK + 4096); // allocate memory for sock chunks
    snprintf(tmp, sizeof(tmp), "%ld", meta_len); // convert file length to string
    SSL_write(client->ssl, tmp, strlen(tmp)); // send file length to the client
    if (SSL_read(client->ssl, tmp, 2) < 0) return; // wait for listening sign

    /*Send file chunks*/
    int fd = open(decomp_path, O_RDONLY|O_LARGEFILE); // open the decompressed file
    if (fd == -1) exitm(WALK_FAIL);
    printf("{%lu} sending %s...\n", hash64(uname), dir_name);
    while (chunk < meta_len) {
        bytes_read = read(fd,((char *)buff + chunk_stat), FILE_CHUNK); // read FILE_CHUNK bytes of data
        chunk_stat += bytes_read; // update the chunk stat
        chunk += bytes_read; // update total bytes read
        if (bytes_read < 0) {
            // Handle reading error
            free(buff);
            close(fd);
            exitm(BROKEN_META);
        }
        if (chunk_stat >= SOCK_CHUNK) {
            /*Buffer is full*/
            total_sent += SSL_write(client->ssl, (char*)buff, chunk_stat); // send buffer
            chunk_stat = 0; // reset chunk stat
            logProgress((int)(((double)total_sent / meta_len)*100)); // log the progress
        }
    }
    if (chunk_stat > 0) {
        /*Buffer isn't empty yet*/
        total_sent += SSL_write(client->ssl, (char*)buff, chunk_stat); // send the remaining data
        logProgress((int)(((double)total_sent / meta_len)*100)); // log the progress
    }
    printf("\n");
    free(buff); // free the buffer
    remove(decomp_path); // remove the decompressed file
}

void sendDirStat(Client *client, const char dir_name[], const char id[]) {
    /*In: a client, folder name, id struct
    Check if a directory exists and response accordingly*/
    char uname[128];
    if (idStat(id) < 0 || splitv(uname, id, "\r\n", 0) != 0) SSL_write(client->ssl, ACCESS_FAIL, 2); // Check if user is registerd
    else {
        char meta_path[1024];
        snprintf(meta_path, sizeof(meta_path), "%s/%lu/%s.zip", APP_DATA, hash64(uname), dir_name); // zip file path
        if (isFile(meta_path)) SSL_write(client->ssl, ACCESS_OK, 2); // check if file exists
        else SSL_write(client->ssl, ACCESS_FAIL, 2); // otherwise send a ACCESS_FAIL response
    }
}

int idStat(const char id[]) {
    /*In: id struct
    >0 Positive value - client has the right password to a registerd user
    =0 Zero value - client is a new user - username wasn't registerd yet
    <0 Negative value - client has a wrong password to a registerd user*/
    char dir_name[256], uname[128], pass[128], root_path[512], id_path[1024];
    if (splitv(uname, id, "\r\n", 0) != 0) exitm(BROKEN_META); // username
    snprintf(root_path, sizeof(root_path), "%s/%lu", APP_DATA, hash64(uname)); // root directory path
    snprintf(id_path, sizeof(id_path), "%s/__id__", root_path); // id file path
    if (!isDir(root_path)) return 0; // no root directory - user doesn't exist
    else {
        char hash_id[64], new_hash_id[64];
        snprintf(new_hash_id, sizeof(new_hash_id), "%lu", hash64(id)); // potential hash generated by the tested id
        readFile(hash_id, id_path, "r"); // read the real hash from the id file
        if (strcmp(hash_id, new_hash_id) == 0) return 1; // password match
        return -1; // password don't match
    }
}

void handleUploadRequest(Client *client, const char id[], size_t meta_len) {
    /*In: a client, id struct, meta length
    Handle the data tranfer
    Process the temp file path to initFolder()*/
    size_t bytes_read, bytes_write, total_bytes = 0;
    char meta_path[512];
    unsigned char *meta_buff = malloc(SOCK_CHUNK + 1024); // allocate data for socket chunks
    snprintf(meta_path, sizeof(meta_path), "/tmp/%ld.meta", hash64(id)); // destination file
    FILE *meta = fopen(meta_path, "wb"); // open the file
    if (meta == NULL) exitm(BROKEN_META);

    /*Read chunks from client
    Write to meta file*/
    while ((bytes_read = SSL_read(client->ssl, meta_buff, SOCK_CHUNK)) > 0) {
        bytes_write = fwrite(meta_buff, 1, bytes_read, meta); // write data
        total_bytes += bytes_write; // update total written bytes
        logProgress((int)(((double)total_bytes / meta_len)*100)); // log progress
        if (total_bytes >= meta_len) break; // break when done
    }
    printf("\n");
    fclose(meta); // close the file
    /*Check if server recieved the entire file*/
    if (meta_len == fileSize(meta_path)) initFolder(client, meta_path, id);
    else printf("Length request conficts with actual meta file.\n");
    free(meta_buff); // free the buffer
    remove(meta_path); // remove the temp meta file
}

void handleAuthRequest(Client *client, const char id[], const char addr_st[]) {
    /*In: client, id struct, addr_st
    Handle the login
    [Note: This function has no impact, but serve clients that
    will to check thier credentials before processing a request]*/
    if (idStat(id) >= 0) {
        SSL_write(client->ssl, ACCESS_OK, 2); // New user/ client has the right password
        printf("Authenticated with %s", addr_st);
    }
    else {
        SSL_write(client->ssl, ACCESS_WP, 2); // Client has a wrong password
        printf("Faied login attempt from %s\n", addr_st);
    }
}

void handleTreeRequest(Client *client, const char id[]) {
    /*In: a client, id struct
    Handle a tree request
    List a folder and send data*/
    int id_stat = idStat(id), count = 0; // get id stat
    char uname[128];
    splitv(uname, id, "\r\n", 0); // get the username
    if (id_stat < 0) {
        /*Attemp to tree with a wrong password
        Send a negative response*/
        SSL_write(client->ssl, "-1", 2);
        printf("{%lu} Failed login attemp\n", hash64(uname));
    }
    else if (id_stat == 0) SSL_write(client->ssl, "00", 2); // Attemp to tree a non-registerd user
    else {
        /*User exists and client has access*/
        char data[SOCK_CHUNK], *tmp, root_path[1024];
        strcpy(data, uname); // build the buffer, username first
        DIR *dir;
        struct dirent *entry;
        snprintf(root_path, sizeof(root_path), "%s/%lu/", APP_DATA, hash64(uname)); // root directory path

        /*Open the root directory
        Read all files*/
        if ((dir = opendir(root_path)) != NULL) {
            while ((entry = readdir(dir)) != NULL) {
                /*Include only zip files (ignore __id__) and unexpected files*/
                tmp = entry->d_name;
                if (
                    strcmp(tmp, ".") * strcmp(tmp, "..") * strcmp(tmp, "__id__") != 0 &&
                    endsWith(tmp, "zip")
                ) {
                    /* add the entry name without the .zip ext */
                    strcat(data, "\n|___ ");
                    strncat(data, tmp, strstr(tmp, ".zip") - tmp); // remove the ext
                    count++; // count directories
                }
            }
            closedir(dir); // close the directory
            tmp = malloc(256); // allocate a temp buffer
            snprintf(tmp, 256, "\n%i Folders", count); // convert count to string
            strcat(data, tmp); // add count to buffer
            snprintf(tmp, sizeof(tmp), "%li", strlen(data)); // convert buffr length to string
            SSL_write(client->ssl, tmp, strlen(tmp)); // send buffer length
            if (SSL_read(client->ssl, tmp, 2) > 0) { // wait for listening sign
                /*Send the data if not empty.
                Otherwise send a negative value*/
                if (strcmp(data, uname) != 0) SSL_write(client->ssl, data, strlen(data));
                else SSL_write(client->ssl, "-1", 2);
            }
            free(tmp); // free the buffer
            printf("{%lu} listed account\n", hash64(uname));
        }
        else {
            /*Unkown error while listing a directory
            Send a zero value*/
            SSL_write(client->ssl, "00", 2);
            printf("[tree] Unkown error...\n");
        }
    }
}

void rmDir(Client *client, const char name[], const char id[]) {
    /*In: a client, dir name, id struct
    Remove a directory from account*/
    char target[1024], uname[128], path[2048];
    splitv(uname, id, "\r\n", 0); // get the username
    snprintf(target, sizeof(target), "%s/%lu/", APP_DATA, hash64(uname)); // target root directory
    if (strcmp(name, "_") == 0) {
        /*Handle -Remove All- option
        /*Open the root directory
        Read all files*/
        DIR *dir;
        struct dirent *entry;
        if ((dir = opendir(target)) != NULL) {
            while ((entry = readdir(dir)) != NULL) {
                if (endsWith(entry->d_name, ".zip")) {
                    strcpy(path, target); // copy the absolute root path
                    strcat(path, entry->d_name); // add the file name
                    remove(path); // remove the zip file
                }
            }
            closedir (dir); // close the directory
        }
        printf("{%lu} Attemp to delete entire account\n", hash64(uname));
    }
    else {
        /*Remove a directory by name*/
        strcat(target, name); // add the file name to root path
        strcat(target, ".zip"); // add the .zip ext
        remove(target); // remove the file
        printf("{%lu} Attemp to delete %s\n", hash64(uname), name);
    }
}

void handleClient(int server, Client *client, SSL_CTX *ctx) {
    /*Recieve client request
    Analyze the input
    Process the request with the required information*/
    size_t bytes, total = 0, written;
    char buff[2048], addr_st[128], id[256];
    snprintf(addr_st, sizeof(addr_st), "%s_%d", inet_ntoa(client->addr.sin_addr), ntohs(client->addr.sin_port)); // client address string
    while (1) {
        /*Read a request
        Max request lenth is 2048 bytes*/
        if ((bytes = SSL_read(client->ssl, buff, sizeof(buff))) != 0) {
            char *line = strtok(buff, "\r\n"); // First field would be the request type
            snprintf(id, sizeof(id), "%s\r\n", (char*)strtok(NULL, "\r\n")); // Second field would be the username
            strcat(id, (char*)strtok(NULL, "\r\n")); // Third field would be the password
            /*Handle the request by type
            The fourth fields is optional and holds a directory name*/
            if (strcmp(line, UPLOAD_REQ) == 0) handleUploadRequest(client, id, atoi(line = strtok(NULL, "\r\n")));
            else if (strcmp(line, DOWNLOAD_REQ) == 0) sendMeta(client, (char*)strtok(NULL, "\r\n"), id);
            else if (strcmp(line, AUTH_REQ) == 0) handleAuthRequest(client, id, addr_st);
            else if (strcmp(line, ISDIR_REQ) == 0) sendDirStat(client, (char*)strtok(NULL, "\r\n"), id);
            else if (strcmp(line, TREE_A_REQ) == 0) handleTreeRequest(client, id);
            else if (strcmp(line, DELETE_REQ) == 0) rmDir(client, (char*)strtok(NULL, "\r\n"), id);
            printf("\n");
        }
        else break; // break when the connection is down
    }
    /*Free the connection*/
    close(client->d);
    SSL_free(client->ssl);
}
