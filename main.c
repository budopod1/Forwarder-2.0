#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include "config.h"
#include "http.h"
#include "pstr.h"
#include "tls.h"

struct addrinfo *targetinfo;

struct PStr *forward(struct PStr *request) {
    int target = socket(targetinfo->ai_family, 
        targetinfo->ai_socktype, targetinfo->ai_protocol);
    if (target == -1) {
        printf("target = socket() error\n");
        exit(1);
    }

    if (connect(target, targetinfo->ai_addr, targetinfo->ai_addrlen)) {
        printf("connect to target error\n");
        exit(1);
    }

#ifdef USE_SSL
    SSL *ssl = upgrade_to_SSL(TARGETHOSTNAME, target);

    if (send_PStr_SSL(ssl, request)) {
        printf("failed to forward request through SSL\n");
        exit(1);
    }
#else
    if (send_PStr(target, request)) {
        printf("failed to forward request\n");
        exit(1);
    }
#endif
    
#ifdef USE_SSL
    set_SSL_recv(ssl);
    recv_PStr recver = &recv_PStr_SSL;
#else
    set_basic_recv_sock(target);
    recv_PStr recver = &recv_PStr_basic;
#endif

    struct PStr *res1 = new_PStr();
    struct PStr *response_headers = recv_headers(res1, recver);
    if (response_headers == NULL) {
        close(target);
        free_PStr(res1);
        return NULL;
    }

    struct ResponseHeaders *headers = (struct ResponseHeaders*)parse_headers(RESPONSE, response_headers);
    if (headers == NULL) {
        close(target);
        free_PStr(res1);
        free_PStr(response_headers);
        return NULL;
    }

    struct PStr *res2 = clone_PStr(res1);

    struct PStr *response_body;
    if (recv_body(res2, response_headers, (struct Headers*)headers, recver, &response_body)) {
        close(target);
        free_PStr(res1);
        free_PStr(res2);
        free_PStr(response_headers);
        free_ResponseHeaders(headers);
        return NULL;
    }

    struct PStr *redirect = get_header((struct Headers*)headers, "location");
    if (redirect != NULL) {
        move_PStr(PStr_replace_once(redirect, TARGETHOSTNAME, strlen(TARGETHOSTNAME), OURHOSTNAME, strlen(OURHOSTNAME)), redirect);
    }

    struct PStr *new_headers = str_response_headers(headers);

    struct PStr *result = build_PStr(
        "%p%s%p", new_headers, REQEND, response_body
    );

    free_PStr(res1);
    free_PStr(res2);
    free_PStr(response_headers);
    free_ResponseHeaders(headers);
    free_PStr(response_body);
    free_PStr(new_headers);

    close(target);

    return result;
}

void accept_request(int server) {
    struct sockaddr_storage remote_addr;
    socklen_t remote_addr_size = sizeof(remote_addr);
    int remote = accept(server, (struct sockaddr*)&remote_addr, &remote_addr_size);
    set_basic_recv_sock(remote);
    recv_PStr recver = &recv_PStr_basic;
    
    struct PStr *req = new_PStr();
    struct PStr *request_headers = recv_headers(req, recver);
    if (request_headers == NULL) {
        close(remote);
        free_PStr(req);
        return;
    }
    
    struct RequestHeaders *headers = (struct RequestHeaders*)parse_headers(REQUEST, request_headers);
    if (headers == NULL) {
        close(remote);
        free_PStr(req);
        free_PStr(request_headers);
        return;
    }
    
    struct PStr *request_body;
    if (recv_body(req, request_headers, (struct Headers*)headers, recver, &request_body)) {
        close(remote);
        free_PStr(req);
        free_PStr(request_headers);
        free_RequestHeaders(headers);
        return;
    }

    remove_header((struct Headers*)headers, "referer");
    
    set_header((struct Headers*)headers, "user-agent", "Mozilla/5.0 (X11; CrOS x86_64 14541.0.0) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/121.0.0.0 Safari/537.36");
    
    struct PStr *target_host = build_PStr("%s:%s", TARGETHOSTNAME, TARGETPORT);
    set_header_PStr((struct Headers*)headers, "host", target_host);
    
    struct PStr *new_headers = str_request_headers(headers);

    struct PStr *forwardee = build_PStr(
        "%p%s%p", new_headers, REQEND, request_body
    );

    free_PStr(req);
    free_PStr(request_headers);
    free_RequestHeaders(headers);
    free_PStr(request_body);
    free_PStr(new_headers);

    struct PStr *response = forward(forwardee);
    
    free_PStr(forwardee);

    send_PStr(remote, response);
    
    free_PStr(response);
    
    close(remote);
}

int main() {
#ifdef USE_SSL
    setup_SSL();
#endif
    
    int getaddrinfo_status;
    
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    getaddrinfo_status = getaddrinfo(TARGETHOSTNAME, TARGETPORT, &hints, &targetinfo);
    if (getaddrinfo_status) {
        printf("getaddrinfo error: %s\n", gai_strerror(getaddrinfo_status));
        exit(1);
    }

    struct addrinfo *serverinfo;

    getaddrinfo_status = getaddrinfo(NULL, SERVEPORT, &hints, &serverinfo);
    if (getaddrinfo_status) {
        printf("getaddrinfo error: %s\n", gai_strerror(getaddrinfo_status));
        exit(1);
    }

    int server = socket(serverinfo->ai_family, 
        serverinfo->ai_socktype, serverinfo->ai_protocol);
    if (server == -1) {
        printf("server = socket() error\n");
        exit(1);
    }

    int yes = 1;
    if (setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1) {
        printf("setsockopt error\n");
        exit(1);
    }
    
    if (bind(server, serverinfo->ai_addr, serverinfo->ai_addrlen)) {
        printf("bind server error\n");
        exit(1);
    }

    if (listen(server, BACKLOG)) {
        printf("listen error\n");
        exit(1);
    }

    while (1) {
        accept_request(server);
    }

    freeaddrinfo(targetinfo);
    freeaddrinfo(serverinfo);
}
