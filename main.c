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
#include <time.h>
#include "config.h"
#include "http.h"
#include "pstr.h"
#include "tls.h"

unsigned int AHH[] = {
    6174, 11777, 7115, 11498, 6248, 6415, 6317, 18272, 8072
};

char *target_hostname;
char *target_port;
int use_ssl;
int send_www;
struct addrinfo *targetinfo = NULL;

int set_target(struct Origin *origin) {
    unsigned int HH = dumb_hash(origin->hostname, strlen(origin->hostname));
    int isA = 0;
    for (unsigned long i = 0; i < sizeof(AHH) / sizeof(AHH[0]); i++) {
        if (HH == AHH[i]) {
            isA = 1;
            break;
        }
    }
    if (!isA) return 1;
    
    use_ssl = uses_SSL(origin->protocol);
    send_www = origin->has_www;
    target_hostname = origin->hostname;
    target_port = get_origin_port(origin);

    freeaddrinfo(targetinfo);

    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    int getaddrinfo_status = getaddrinfo(target_hostname, target_port, &hints, &targetinfo);
    if (getaddrinfo_status) {
        printf("getaddrinfo error: %s\n", gai_strerror(getaddrinfo_status));
        exit(1);
    }

    return 0;
}

void make_empty_ResponseHeaders(struct ResponseHeaders *responseHeaders, int status) {
    responseHeaders->http_version = HTTP11;
    responseHeaders->status = PStr_from_int_len(status, 3);

    responseHeaders->count = 0;
    responseHeaders->headers = NULL;
}

void make_HTTP_date(char buffer[64]) {
    time_t now;
    time(&now);
    struct tm *timeinfo = gmtime(&now);
    strftime(buffer, 63, "%a, %d %b %Y %T %Z", timeinfo);
}

int send_special_response(int remote, struct PStr *response) {
    int send_status = send_PStr(remote, response);
    free_PStr(response);
    if (send_status) {
        printf("Failed to send special url response\n");
        return 1;
    }

    return 0;
}

int serve_file(int remote, struct RequestHeaders *request, int status, char *path, enum ContentType contentType) {
    // https://stackoverflow.com/a/14002993
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        printf("Failed to find requested file %s\n", path);
        return 1;
    }
    
    fseek(file, 0, SEEK_END);
    int fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *bodyBuffer = malloc(fileSize);
    if (fread(bodyBuffer, fileSize, 1, file) < 1) {
        printf("Failed to load requested file %s\n", path);
        return 1;
    }
    fclose(file);
    
    struct PStr body;
    body.text = bodyBuffer;
    body.length = fileSize;
    body.capacity = -1;

    struct ResponseHeaders responseHeaders;
    make_empty_ResponseHeaders(&responseHeaders, status);

    char date[64];
    make_HTTP_date(date);

    char *contentLength = CStr_from_int(fileSize);
    add_headers((struct Headers*)&responseHeaders, 3, (char*[]) {
        "connection",
        "content-type",
        "date",
        "server",
        "content-length"
    }, (char*[]) {
        "close",
        str_content_type(contentType),
        date,
        SERVERNAME,
        contentLength
    });
    free(contentLength);

    struct PStr *responseHeadersTxt = str_response_headers(&responseHeaders);
    struct PStr *response = build_PStr("%p"HEADERSEND"%p", responseHeadersTxt, &body);

    free_PStr(responseHeadersTxt);
    free(bodyBuffer);

    return send_special_response(remote, response);
}

int serve_redirect(int remote, struct RequestHeaders *headers, int status, char *url) {
    struct ResponseHeaders responseHeaders;
    make_empty_ResponseHeaders(&responseHeaders, status);

    char date[64];
    make_HTTP_date(date);

    add_headers((struct Headers*)&responseHeaders, 4, (char*[]) {
        "connection",
        "date",
        "server",
        "location"
    }, (char*[]) {
        "close",
        date,
        SERVERNAME,
        url
    });

    struct PStr *responseHeadersTxt = str_response_headers(&responseHeaders);
    struct PStr *response = build_PStr("%p"HEADERSEND, responseHeadersTxt);
    
    free_PStr(responseHeadersTxt);

    return send_special_response(remote, response);
}

int serve_empty(int remote, int status) {
    struct ResponseHeaders responseHeaders;
    make_empty_ResponseHeaders(&responseHeaders, status);

    char date[64];
    make_HTTP_date(date);

    add_headers((struct Headers*)&responseHeaders, 4, (char*[]) {
        "connection",
        "content-type",
        "date",
        "server",
        "content-length"
    }, (char*[]) {
        "close",
        str_content_type(TEXTPLAIN_CONTENTTYPE),
        date,
        SERVERNAME,
        "0"
    });

    struct PStr *responseHeadersTxt = str_response_headers(&responseHeaders);
    struct PStr *response = build_PStr("%p"HEADERSEND, responseHeadersTxt);

    free_PStr(responseHeadersTxt);

    return send_special_response(remote, response);
}

int serve_request(int remote, struct RequestHeaders *headers, struct PStr *body) {
    if (CStr_equals_PStr(SPECIALURL VIEWURL, headers->url)) {
        return serve_file(remote, headers, 200, "web/view.html", TEXTHTML_CONTENTTYPE);
    } else if (CStr_equals_PStr(SPECIALURL NEWTABURL, headers->url)) {
        return serve_file(remote, headers, 200, "web/new_tab.html", TEXTHTML_CONTENTTYPE);
    } else if (CStr_equals_PStr(SPECIALURL STYLEURL, headers->url)) {
        return serve_file(remote, headers, 200, "web/style.css", TEXTCSS_CONTENTTYPE);
    } else if (CStr_equals_PStr(SPECIALURL SCRIPTURL, headers->url)) {
        return serve_file(remote, headers, 200, "web/script.js", TEXTJS_CONTENTTYPE);
    } else if (CStr_equals_PStr(SPECIALURL CHANGEORIGINURL, headers->url)) {
        struct Origin *origin = parse_origin(body);
        if (origin == NULL) return 1;
        int set_target_status = set_target(origin);
        free(origin);
        if (set_target_status) {
            return serve_empty(remote, 403);
        } else {
            return serve_empty(remote, 200);
        }
    } else if (CStr_equals_PStr(SPECIALURL FAVICONURL, headers->url)) {
        return serve_file(remote, headers, 200, "web/favicon.ico", IMAGEICON_CONTENTTYPE);
    } else {
        return serve_file(remote, headers, 404, "web/404.html", TEXTHTML_CONTENTTYPE);
    }
}

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

    SSL *ssl;
    recv_PStr recver;
    
    if (use_ssl) {
        ssl = upgrade_to_SSL(target_hostname, target);
    
        if (ssl == NULL) return NULL;
    
        if (send_PStr_SSL(ssl, request)) {
            printf("failed to forward request through SSL\n");
            return NULL;
        }

        set_SSL_recv(ssl);
        recver = &recv_PStr_SSL;
    } else {
        if (send_PStr(target, request)) {
            printf("failed to forward request\n");
            return NULL;
        }
        
        set_basic_recv_sock(target);
        recver = &recv_PStr_basic;
    }

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

    if (headers->http_version != HTTP11) {
        close(target);
        free_PStr(res1);
        free_PStr(response_headers);
        free_ResponseHeaders(headers);
        printf("Only http version 1.1 is supported for responses\n");
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

    set_header((struct Headers*)headers, "cache-control", "no-store");
    remove_header((struct Headers*)headers, "expires");

    remove_header((struct Headers*)headers, "strict-transport-security");
    remove_header((struct Headers*)headers, "content-security-policy");
    // backwards compatibility:
    remove_header((struct Headers*)headers, "x-content-security-policy");
    remove_header((struct Headers*)headers, "x-frame-options");

    struct PStr *redirect = get_header((struct Headers*)headers, "location");
    if (redirect != NULL) {
        struct PStr *wOurHostname = PStr_replace_once(
            redirect, target_hostname, strlen(target_hostname),
            OURHOSTNAME, strlen(OURHOSTNAME)
        );
        move_PStr(PStr_remove_once(wOurHostname, "www.", 4, &send_www), redirect);
        free_PStr(wOurHostname);
    }

    struct PStr *new_headers = str_response_headers(headers);

    struct PStr *result = build_PStr(
        "%p"HEADERSEND"%p", new_headers, response_body
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

    struct PStr *req1 = new_PStr();
    struct PStr *request_headers = recv_headers(req1, recver);
    if (request_headers == NULL) {
        close(remote);
        free_PStr(req1);
        return;
    }

    struct RequestHeaders *headers = (struct RequestHeaders*)parse_headers(REQUEST, request_headers);
    if (headers == NULL) {
        close(remote);
        free_PStr(req1);
        free_PStr(request_headers);
        return;
    }

    if (headers->http_version != HTTP11) {
        close(remote);
        free_PStr(req1);
        free_PStr(request_headers);
        free_RequestHeaders(headers);
        printf("Only http version 1.1 is supported for requests\n");
        return;
    }

    struct PStr *req2 = clone_PStr(req1);
    struct PStr *request_body;
    if (recv_body(req2, request_headers, (struct Headers*)headers, recver, &request_body)) {
        close(remote);
        free_PStr(req1);
        free_PStr(req2);
        free_PStr(request_headers);
        free_RequestHeaders(headers);
        return;
    }

    if (PStr_starts_with(headers->url, SPECIALURL, strlen(SPECIALURL))) {
        serve_request(remote, headers, request_body);

        free_PStr(req1);
        free_PStr(req2);
        free_PStr(request_headers);
        free_RequestHeaders(headers);
        free_PStr(request_body);

        close(remote);
    } else if (targetinfo == NULL) {
        serve_redirect(remote, headers, 307, SPECIALURL VIEWURL);

        free_PStr(req1);
        free_PStr(req2);
        free_PStr(request_headers);
        free_RequestHeaders(headers);
        free_PStr(request_body);

        close(remote);
    } else {
        remove_header((struct Headers*)headers, "referer");

        set_header((struct Headers*)headers, "user-agent", "Mozilla/5.0 (X11; CrOS x86_64 14541.0.0) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/121.0.0.0 Safari/537.36");

        set_header((struct Headers*)headers, "accept-encoding", "identity");

        char *host_template;
        if (send_www) {
            host_template = "www.%s:%s";
        } else {
            host_template = "%s:%s";
        }
        struct PStr *target_host = build_PStr(
            host_template, target_hostname, target_port
        );
        set_header_PStr((struct Headers*)headers, "host", target_host);

        struct PStr *new_headers = str_request_headers(headers);
        
        free_PStr(request_headers);
        free_RequestHeaders(headers);

        struct PStr *forwardee = build_PStr(
            "%p"HEADERSEND"%p", new_headers, request_body
        );
        free_PStr(req1);
        free_PStr(req2);
        free_PStr(new_headers);
        free_PStr(request_body);

        struct PStr *response = forward(forwardee);

        free_PStr(forwardee);

        if (response == NULL) {
            close(remote);
            printf("NULL response\n");
            return;
        }

        int send_status = send_PStr(remote, response);
        free_PStr(response);
        close(remote);
        if (send_status) {
            printf("Failed to send response to client\n");
            return;
        }
    }
}

int main() {
    setup_SSL();
    
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    struct addrinfo *serverinfo;

    int getaddrinfo_status = getaddrinfo(NULL, SERVEPORT, &hints, &serverinfo);
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
