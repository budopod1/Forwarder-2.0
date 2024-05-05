#define HTTP_SRC
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include "config.h"
#include "pstr.h"
#include "utils.h"
#include "http.h"

char *METHODSTXT[] = {"GET", "POST", "HEAD", "OPTIONS"};
char *VERSIONSTXT[] = {"HTTP/1.0", "HTTP/1.1", "HTTP/2.0"};

void free_RequestHeaders(struct RequestHeaders *headers) {
    free_PStr(headers->url);
    for (int i = 0; i < headers->count; i++) {
        free_PStrPair((struct PStrPair*)headers->headers[i]);
    }
    free(headers->headers);
    free(headers);
}

void free_ResponseHeaders(struct ResponseHeaders *headers) {
    free_PStr(headers->status);
    for (int i = 0; i < headers->count; i++) {
        free_PStrPair((struct PStrPair*)headers->headers[i]);
    }
    free(headers->headers);
    free(headers);
}

int BASIC_RECV_SOCK;

void set_basic_recv_sock(int sock) {
    BASIC_RECV_SOCK = sock;
}

int recv_PStr_basic(struct PStr *str) {
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
    int bytes_recv = recv(BASIC_RECV_SOCK, str->text+old_len, MAX_RECV, 0);
    if (bytes_recv == 0) return 1;
    str->length = old_len + bytes_recv;
    return 0;
}

int send_PStr(int sock, struct PStr *str) {
    int sent = 0;
    while (sent < str->length) {
        int n = send(sock, str->text + sent, str->length - sent, 0);
        if (n == -1) return 1;
        sent += n;
    }
    return 0;
}

char *str_method(enum HTTPMethod method) {
    return METHODSTXT[method];
}

char *str_http_version(enum HTTPVersion version) {
    return VERSIONSTXT[version];
}

struct PStr *str_header_list(const struct Headers *headers) {
    struct PStrList *lines = malloc(sizeof(struct PStrList));
    int header_count = headers->count;
    lines->count = header_count;
    lines->items = malloc(sizeof(struct PStr) * header_count);
    for (int i = 0; i < header_count; i++) {
        struct Header *header = headers->headers[i];
        struct PStr *built = build_PStr("%p: %p", &header->key, &header->value);
        struct PStr *target_loc = lines->items+i;
        memcpy(target_loc, built, sizeof(struct PStr));
        free(built);
    }
    struct PStr *result = join_PStrList(lines, "\r\n", 2);
    free_PStrList(lines);
    return result;
}

struct PStr *str_request_headers(struct RequestHeaders *headers) {
    char *method = str_method(headers->method);
    char *version = str_http_version(headers->http_version);
    struct PStr *header_list = str_header_list((struct Headers*)headers);
    struct PStr *result = build_PStr(
        "%s %p %s\r\n%p", method, headers->url, version, header_list
    );
    free_PStr(header_list);
    return result;
}

struct PStr *str_response_headers(struct ResponseHeaders *headers) {
    char *version = str_http_version(headers->http_version);
    struct PStr *header_list = str_header_list((struct Headers*)headers);
    struct PStr *result = build_PStr(
        "%s %p\r\n%p", version, headers->status, header_list
    );
    free_PStr(header_list);
    return result;
}

enum HTTPMethod parse_method(struct PStr *str) {
    return parse_enum(str, METHODSTXT, UNKNOWNMETHOD);
}

enum HTTPVersion parse_http_version(struct PStr *str) {
    return parse_enum(str, VERSIONSTXT, UNKNOWNVERSION);
}

int parse_request_start_line(struct RequestHeaders *headers, struct PStr *start_line) {
    struct PStrList *parts = split_PStr(start_line, " ", 1);
    if (parts->count != 3) {
        free_PStrList(parts);
        printf_PStr("Invalid start line %p\n", start_line);
        return 1;
    }

    enum HTTPMethod method = parse_method(parts->items + 0);
    if (method == UNKNOWNMETHOD) {
        free_PStrList(parts);
        printf_PStr("Unknown http method %p\n", parts->items + 0);
        return 1;
    }
    headers->method = method;

    headers->url = clone_PStr(parts->items + 1);

    enum HTTPVersion http_version = parse_http_version(parts->items + 2);
    free_PStrList(parts);
    if (http_version == UNKNOWNVERSION) {
        free_PStr(headers->url);
        printf_PStr("Unknown http version %p\n", parts->items + 2);
        return 1;
    }
    headers->http_version = http_version;

    return 0;
}

int parse_response_start_line(struct ResponseHeaders *headers, struct PStr *start_line) {
    struct PStrPair *pair = partition_PStr(start_line, " ");
    if (pair == NULL) {
        printf_PStr("Invalid start line %p\n", start_line);
        return 1;
    }

    enum HTTPVersion version = parse_http_version(&pair->first);
    if (version == UNKNOWNVERSION) {
        free_PStrPair(pair);
        printf_PStr("Unknown http version %p\n", &pair->first);
        return 1;
    }
    headers->http_version = version;

    headers->status = clone_PStr(&pair->second);
    free_PStrPair(pair);
    return 0;
}

struct Header *parse_header(struct PStr *txt) {
    struct PStrPair *pair = partition_PStr(txt, ": ");
    if (pair == NULL) {
        printf_PStr("Invalid http header: %p\n", txt);
        return NULL;
    }
    struct PStr *key = &pair->first;
    memcpy(&pair->first, PStr_to_lower(key), sizeof(struct PStr));
    // the old key's capacity will alway's be -1, so it doesn't need to be freed
    return (struct Header*)pair;
}

struct Headers *parse_headers(int isRequest, struct PStr *txt) {
    struct Headers *headers = malloc(isRequest ? sizeof(struct RequestHeaders) : sizeof(struct ResponseHeaders));
    int headerCount = 0;
    struct Header **header_list = NULL;
    struct PStrList *list = split_PStr(txt, "\r\n", 2);
    for (int i = 0; i < list->count; i++) {
        struct PStr *header_txt = list->items + i;
        if (i == 0) {
            if (isRequest) {
                if (parse_request_start_line((struct RequestHeaders*)headers, header_txt)) {
                    free_PStrList(list);
                    return NULL;
                }
            } else {
                if (parse_response_start_line((struct ResponseHeaders*)headers, header_txt)) {
                    free_PStrList(list);
                    return NULL;
                }
            }
            continue;
        }
        struct Header *header = parse_header(header_txt);
        if (header == NULL) {
            free_PStrList(list);
            return NULL;
        }
        int headerMem = sizeof(struct Header*) * ++headerCount;
        header_list = realloc(header_list, headerMem);
        header_list[headerCount-1] = header;
    }
    headers->headers = header_list;
    headers->count = headerCount;
    free_PStrList(list);
    return headers;
}

void remove_header(struct Headers *headers, char *removee) {
    for (int i = 0; i < headers->count; i++) {
        struct Header *header = headers->headers[i];
        if (CStr_equals_PStr(removee, &header->key)) {
            free_PStrPair((struct PStrPair*)header);
            int size = (--headers->count-i)*sizeof(struct Header*);
            memcpy(headers->headers+i, headers->headers+i+1, size);
            i--;
        }
    }
}

struct PStr *get_header(struct Headers *headers, char *key) {
    for (int i = 0; i < headers->count; i++) {
        struct Header *header = headers->headers[i];
        if (CStr_equals_PStr(key, &header->key)) {
            return &header->value;
        }
    }
    return NULL;
}

void set_header(struct Headers *headers, char *key, char *value) {
    struct PStr *old_val = get_header(headers, key);
    if (old_val == NULL) {
        headers->headers = realloc(headers->headers, ++headers->count*sizeof(struct Header*));
        struct Header *header = malloc(sizeof(struct Header));
        CStr_copy_to_PStr(key, &header->key);
        CStr_copy_to_PStr(value, &header->value);
        headers->headers[headers->count-1] = header;
    } else {
        CStr_copy_to_PStr(value, old_val);
    }
}

void set_header_PStr(struct Headers *headers, char *key, struct PStr *value) {
    struct PStr *old_val = get_header(headers, key);
    if (old_val == NULL) {
        headers->headers = realloc(headers->headers, ++headers->count*sizeof(struct Header*));
        struct Header *header = malloc(sizeof(struct Header));
        CStr_copy_to_PStr(key, &header->key);
        copy_to_PStr(value, &header->value);
        headers->headers[headers->count-1] = header;
    } else {
        copy_to_PStr(value, old_val);
    }
}

char *HEADERSEND = "\r\n\r\n";

struct PStr *recv_headers(struct PStr *req, recv_PStr recver) {
    int i = 0;
    while (1) {
        if (recver(req)) {
            printf("Connection unexpectedly closed while receiving headers\n");
            return NULL;
        }
        int req_end_len = strlen(HEADERSEND);
        int max_i = req->length + req_end_len - 1;
        for (; i < max_i; i++) {
            if (memcmp(req->text + i, HEADERSEND, req_end_len) == 0) {
                return slice_PStr(req, 0, i);
            }
        }
    }
}

int recv_body(struct PStr *req, struct PStr *headersTxt, struct Headers *headers, recv_PStr recver, struct PStr **request_body) {
    *request_body = new_PStr();
    struct PStr *transferEncoding = get_header(headers, "transfer-encoding");
    if (transferEncoding != NULL) {
        if (!CStr_equals_PStr("identity", transferEncoding)) {
            printf_PStr("%p transfer encoding not supported\n", transferEncoding);
            return 1;
        }
    }
    struct PStr *contentLengthTxt = get_header(headers, "content-length");
    if (contentLengthTxt == NULL) {
        return 0;
    }
    contentLengthTxt = clone_PStr(contentLengthTxt);
    null_terminate_PStr(contentLengthTxt);
    char *contentLengthEnd;
    int contentLength = strtol(contentLengthTxt->text, &contentLengthEnd, 10);
    free_PStr(contentLengthTxt);
    if (contentLengthTxt->text == contentLengthEnd) {
        printf("Invalid Content-Length\n");
        return 1;
    }
    int ogLength = headersTxt->length + strlen(REQEND);
    while (req->length < contentLength + ogLength) {
        if (recver(req)) {
            printf("Connection unexpectedly closed while receiving body\n");
            return 1;
        }
    }
    *request_body = slice_PStr(req, ogLength, contentLength);
    return 0;
}
