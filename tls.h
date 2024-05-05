#include <openssl/ssl.h>

#ifndef TLS_H
#define TLS_H

void setup_SSL();

SSL *upgrade_to_SSL(char *hostname, int sock);

void set_SSL_recv(SSL *ssl);

int recv_PStr_SSL(struct PStr *str);

int send_PStr_SSL(SSL *ssl, struct PStr *str);

void close_SSL(SSL *ssl);

void shutdown_SSL();

#endif
