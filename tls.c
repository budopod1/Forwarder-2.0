#include <stdio.h>
#include <string.h>
#include <openssl/ssl.h>
#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/conf.h>
#include <openssl/x509.h>
#include <openssl/x509_vfy.h>
#include <threads.h>
#include "pstr.h"
#include "config.h"

// https://codereview.stackexchange.com/a/264413

SSL_CTX *ctx;

void setup_SSL() {
    SSL_library_init();

    const SSL_METHOD *method = TLS_method();
    if (!method) {
        printf("SSL TLS method failed\n");
        exit(1);
    }
    
    ctx = SSL_CTX_new(method);
    if (!ctx) {
        printf("SSL CTX creation failed\n");
        exit(1);
    }
    
    SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, NULL);
    SSL_CTX_set_verify_depth(ctx, 4);
    SSL_CTX_set_options(ctx, SSL_OP_ALL);
    if (SSL_CTX_set_default_verify_paths(ctx) == 0) {
        printf("Couldn't set default verify paths\n");
        exit(1);
    }
}

SSL *upgrade_to_SSL(char *hostname, int sock) {
    SSL *ssl = SSL_new(ctx);
    if (ssl == NULL) {
        printf("Couldn't create SSL\n");
        exit(1);
    }

    const char *const PREFERED_CIPHERS = "HIGH:!aNULL:!kRSA:!PSK:!SRP:!MD5:!RC4";
    if (SSL_set_cipher_list(ssl, PREFERED_CIPHERS) != 1) {
        printf("Couldn't set cipher list\n");
        exit(1);
    }

    if (SSL_set_tlsext_host_name(ssl, hostname) != 1) {
        printf("Hostname error\n");
        exit(1);
    }
    
    // int ssl_sock = SSL_get_fd(ssl);
    if (SSL_set_fd(ssl, sock) == 0) {
        printf("SLL set file descriptor error\n");
        exit(1);
    }
    
    int SSL_status = SSL_connect(ssl);
    switch (SSL_get_error(ssl, SSL_status)) {
        case SSL_ERROR_NONE:
            break;
        case SSL_ERROR_ZERO_RETURN:
            printf("Peer has closed connection\n");
            exit(1);
        case SSL_ERROR_SSL:
            ERR_print_errors_fp(stderr);
            SSL_shutdown(ssl);
            printf("Internal SSL error\n");
            return NULL;
        default:
            ERR_print_errors_fp(stderr);
            printf("Unknown SSL error\n");
            exit(1);
    }
    
    return ssl;
}

thread_local SSL *SSL_RECV;

void set_SSL_recv(SSL *ssl) {
    SSL_RECV = ssl;
}

int recv_PStr_SSL(struct PStr *str) {
    int old_len = str->length;
    int possible_len = old_len + MAX_RECV;
    if (str->capacity < 0) {
        printf("Cannot recv to dependent PStr\n");
        exit(1);
    }
    if (possible_len > str->capacity) {
        str->text = realloc(str->text, possible_len);
        str->capacity = possible_len;
    }
    int bytes_recv = SSL_read(SSL_RECV, str->text+old_len, MAX_RECV);
    if (bytes_recv <= 0) return 1;
    str->length = old_len + bytes_recv;
    return 0;
}

int send_PStr_SSL(SSL *ssl, struct PStr *str) {
    int sent = 0;
    while (sent < str->length) {
        int n = SSL_write(ssl, str->text + sent, str->length - sent);
        if (n <= 0) return 1;
        sent += n;
    }
    return 0;
}

void close_SSL(SSL *ssl) {
    SSL_free(ssl);
}

void shutdown_SSL() {
    SSL_CTX_free(ctx);
}
