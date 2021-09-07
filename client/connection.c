#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <malloc.h>
#include <string.h>
#include <sys/socket.h>
#include <resolv.h>
#include <netdb.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include "include/connection.h"
#include "include/exit.h"
#include "include/settings.h"

int bindAddr(const char *hostname, int port) {
    // bind address to socket
    struct hostent *host;
    struct sockaddr_in addr;
    if ((host = gethostbyname(hostname)) == NULL) exitm(CONNECT_ERR); // get host by name
    int sd = socket(PF_INET, SOCK_STREAM, 0); // create client descriptor
    bzero(&addr, sizeof(addr)); // memset address with 0
    addr.sin_family = AF_INET; // IPv4 address family
    addr.sin_port = htons(port); // convert to network short byte order
    addr.sin_addr.s_addr = *(long*)(host->h_addr); // set the IP of the socket; sin_addr is an union
    if (connect(sd, (struct sockaddr*)&addr, sizeof(addr)) != 0) exitm(CONNECT_ERR); // connect to host
    return sd;
}

SSL_CTX* InitClientCTX(void) {
    // create server ssl context
    OpenSSL_add_all_algorithms(); // set cryptos
    SSL_load_error_strings(); // set error messages
    const SSL_METHOD *method = TLS_client_method(); // create client method
    SSL_CTX *ctx = SSL_CTX_new(method); // create client context
    if (ctx == NULL) ERR_print_errors_fp(stderr); // print error
    return ctx;
    exitm(CONNECT_ERR);
}
