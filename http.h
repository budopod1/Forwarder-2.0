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

enum HTTPVersion {
    HTTP10,
    HTTP11,
    HTTP20,
    UNKNOWNVERSION
};

enum TransferEncoding {
    identity_TRANSFERENCODING = 0,
    chunked_TRANSFERENCODING = 1,
    compress_TRANSFERENCODING = 2,
    deflate_TRANSFERENCODING = 4,
    gzip_TRANSFERENCODING = 8,
    UNKNOWNTRANSFERENCODING = 16
};

enum ContentType {
    TEXTPLAIN_CONTENTTYPE,
    TEXTHTML_CONTENTTYPE,
    TEXTCSS_CONTENTTYPE,
    TEXTJS_CONTENTTYPE,
    IMAGEICON_CONTENTTYPE
};

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

enum Protocol_ {
    HTTP, HTTPS, UNKNOWNPROTOCOL
};

struct Origin {
    enum Protocol_ protocol;
    bool has_www;
    char *hostname;
    char *port;
};

void free_RequestHeaders(struct RequestHeaders *headers);

void free_ResponseHeaders(struct ResponseHeaders *headers);

void set_basic_recv_sock(int sock);

int recv_PStr_basic(struct PStr *str);

int send_PStr(int sock, struct PStr *str);

char *str_content_type(enum ContentType contentType);

char *str_method(enum HTTPMethod method);

char *str_http_version(enum HTTPVersion version);

struct PStr *str_request_headers(struct RequestHeaders *headers);

struct PStr *str_response_headers(struct ResponseHeaders *headers);

int uses_SSL(enum Protocol_ protocol);

char *get_origin_port(struct Origin *origin);

struct Headers *parse_headers(int isRequest, struct PStr *txt);

struct Origin *parse_origin(struct PStr *text);

void remove_header(struct Headers *headers, char *removee);

struct PStr *get_header(struct Headers *headers, char *key);

void set_header(struct Headers *headers, char *key, char *value);

void set_header_PStr(struct Headers *headers, char *key, struct PStr *value);

void add_header(struct Headers *headers, char *key, struct PStr *value);

void add_headers(struct Headers *headers, int count, char *keys[], char *values[]);

struct PStr *recv_headers(struct PStr *req, recv_PStr recver);

int recv_body(struct PStr *req, struct PStr *headersTxt, struct Headers *headers, recv_PStr recver, struct PStr **request_body);

#endif
