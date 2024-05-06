#include "pstr.h"

#ifndef HTTP_H
#define HTTP_H

#define HTTPNEWLINE "\r\n"
#define HTTPNEWLINESIZE 2
#define HEADERSEND "\r\n\r\n"
#define HEADERSENDSIZE 4

struct Header {
    struct PStr key;
    struct PStr value;
};

enum HTTPMethod {
    GET, POST, HEAD, OPTIONS, UNKNOWNMETHOD
};
#ifndef HTTP_SRC
extern char *METHODSTXT[];
#endif

enum HTTPVersion {
    HTTP10,
    HTTP11,
    HTTP20,
    UNKNOWNVERSION
};
#ifndef HTTP_SRC
extern char *VERSIONSTXT[];
#endif

enum TransferEncoding {
    identity_TRANSFERENCODING = 0,
    chunked_TRANSFERENCODING = 1,
    compress_TRANSFERENCODING = 2,
    deflate_TRANSFERENCODING = 4,
    gzip_TRANSFERENCODING = 8,
    UNKNOWNTRANSFERENCODING = 16
};
#ifndef HTTP_SRC
extern char *TRANSFERENCODINGSTXT[];
#endif

struct Headers {
    enum HTTPVersion http_version;
    int count;
    struct Header **headers;
};

struct RequestHeaders {
    enum HTTPVersion http_version;
    int count;
    struct Header **headers;
    //
    struct PStr *url;
    enum HTTPMethod method;
};

struct ResponseHeaders {
    enum HTTPVersion http_version;
    int count;
    struct Header **headers;
    //
    struct PStr *status;
};

void free_RequestHeaders(struct RequestHeaders *headers);

void free_ResponseHeaders(struct ResponseHeaders *headers);

void set_basic_recv_sock(int sock);

int recv_PStr_basic(struct PStr *str);

int send_PStr(int sock, struct PStr *str);

char *str_method(enum HTTPMethod method);

char *str_http_version(enum HTTPVersion version);

struct PStr *str_request_headers(struct RequestHeaders *headers);

struct PStr *str_response_headers(struct ResponseHeaders *headers);

struct Headers *parse_headers(int isRequest, struct PStr *txt);

void remove_header(struct Headers *headers, char *removee);

struct PStr *get_header(struct Headers *headers, char *key);

void set_header(struct Headers *headers, char *key, char *value);

void set_header_PStr(struct Headers *headers, char *key, struct PStr *value);

struct PStr *recv_headers(struct PStr *req, recv_PStr recver);

int recv_body(struct PStr *req, struct PStr *headersTxt, struct Headers *headers, recv_PStr recver, struct PStr **request_body);

#endif
