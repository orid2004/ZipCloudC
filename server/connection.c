#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <malloc.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <resolv.h>
#include "openssl/ssl.h"
#include "openssl/err.h"
#include "include/settings.h"
#include "include/connection.h"
#include "include/exit.h"

int bindAddr(int port) {
    /* In: port num
    Bind address to socket
    Return the descriptor*/
    struct sockaddr_in addr;
    int sd = socket(PF_INET, SOCK_STREAM, 0); // server descriptor
    bzero(&addr, sizeof(addr)); // memset addr to zero --
    addr.sin_family = AF_INET; // IPv4 address family
    addr.sin_port = htons(port); // convert to network short byte order
    addr.sin_addr.s_addr = INADDR_ANY; // set the IP of the socket / sin_addr is an union
    if (bind(sd, (struct sockaddr*)&addr, sizeof(addr)) != 0) exitm(BIND_ERR); // bind address
    if (listen(sd, 10) != 0) exitm(LISTEN_ERR); // set listen queue
    return sd; // -- return descriptor
}

void useCertificate(SSL_CTX *ctx, char *CertFile, char *KeyFile) {
    /*In: context, certificate, key
    Use the keys*/
    if (SSL_CTX_use_certificate_file(ctx, CertFile, SSL_FILETYPE_PEM) <= 0) exitm(SSL_KEYS_ERR);
    else if (SSL_CTX_use_PrivateKey_file(ctx, KeyFile, SSL_FILETYPE_PEM) <= 0) exitm(SSL_KEYS_ERR);
    else if (!SSL_CTX_check_private_key(ctx)) exitm(SSL_KEYS_ERR); // check private key
}

SSL_CTX* InitServerCTX(void) {
    /*Create a server context*/
    OpenSSL_add_all_algorithms();  // set cryptos
    SSL_load_error_strings();   // set error messages
    const SSL_METHOD *method = TLS_server_method();  // create server method
    SSL_CTX *ctx = SSL_CTX_new(method);   // create server context
    if (ctx == NULL) ERR_print_errors_fp(stderr);
    else return ctx;
    exitm(ABORT);
}


void closeSSL(SSL *ssl) {
    // close client ssl
    SSL_free(ssl);
    close(SSL_get_fd(ssl));
}

void closeServerSSL(int server, SSL_CTX *ctx) {
    // close server ssl
    close(server);
    SSL_CTX_free(ctx); // release context
}
