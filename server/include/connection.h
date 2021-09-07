/*Header for server/connection.c file*/
int bindAddr(int port);
void useCertificate(SSL_CTX* ctx, char* CertFile, char* KeyFile);
SSL_CTX* InitServerCTX(void);
void startServer(SSL_CTX *ctx, int *server, int port);
void closeSSL(SSL* ssl);
void closeServerSSL(int server, SSL_CTX* ctx);
