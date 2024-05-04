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

char *TARGETHOSTNAME = "en.wikipedia.com";
char *TARGETPORT = "80";

struct addrinfo *targetinfo;

char *SERVEPORT = "8080";
int BACKLOG = 10;
int MAX_RECV = 1024;

int REQUEST = 1;
int RESPONSE = 0;

struct PStr {
    // if capacity is -1: PStr is dependent, memory is managed elsewhere
    // if capacity is -2: PStr is parent of dependencies, memory is managed here
    int capacity;
    int length;
    char *text;
};

struct PStrList {
    int count;
    struct PStr *items;
};

struct PStrPair {
    struct PStr first;
    struct PStr second;
};

struct Header {
    struct PStr key;
    struct PStr value;
};

enum HTTPMethod {
    GET, POST, HEAD, OPTIONS, UNKNOWNMETHOD
};

char *METHODSTXT[] = {"GET", "POST", "HEAD", "OPTIONS"};

enum HTTPVersion {
    HTTP10,
    HTTP11,
    HTTP20,
    UNKNOWNVERSION
};
char *VERSIONSTXT[] = {"HTTP/1.0", "HTTP/1.1", "HTTP/2.0"};

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

struct PStr *new_PStr() {
    struct PStr *str = malloc(sizeof(struct PStr));
    memset(str, 0, sizeof(struct PStr));
    return str;
}

void free_PStr(struct PStr *str) {
    if (str == NULL) return;
    if (str->capacity != -1) {
        free(str->text);
    }
    free(str);
}

void free_PStrList(struct PStrList *list) {
    if (list == NULL) return;
    for (int i = 0; i < list->count; i++) {
        struct PStr *str = list->items + i;
        if (str->capacity != -1) {
            free(str->text);
        }
    }
    free(list->items);
    free(list);
}

void free_PStrPair(struct PStrPair *pair) {
    if (pair == NULL) return;
    if (pair->first.capacity != -1) {
        free(pair->first.text);
    }
    if (pair->second.capacity != -1) {
        free(pair->second.text);
    }
    free(pair);
}

void print_PStr(struct PStr *str) {
    fwrite(str->text, 1, str->length, stdout);
}

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

void null_terminate_PStr(struct PStr *str) {
    if (str->capacity < 0) {
        printf("Cannot null terminate dependent PStr\n");
        exit(1);
    }
    if (str->capacity == str->length) {
        str->text = realloc(str->text, str->capacity++);
    }
    str->text[str->length] = '\0';
}

struct PStr *slice_PStr(struct PStr *source, int start, int len) {
    struct PStr *str = malloc(sizeof(struct PStr));
    str->capacity = -1;
    str->length = len;
    str->text = source->text + start;
    if (source->capacity != -1) source->capacity = -2;
    return str;
}

struct PStrList *split_PStr(struct PStr *txt, char *splitter) {
    int splitter_len = strlen(splitter);
    struct PStrList *list = malloc(sizeof(struct PStrList));
    int count = 0;
    list->items = NULL;
    int secstart = 0;
    for (int i = 0; i <= txt->length - splitter_len; i++) {
        if (memcmp(txt->text + i, splitter, splitter_len) == 0) {
            list->items = realloc(list->items, ++count*sizeof(struct PStr));
            struct PStr *new_val = list->items + count - 1;
            new_val->capacity = -1;
            new_val->length = i - secstart;
            new_val->text = txt->text + secstart;
            i += splitter_len;
            secstart = i;
        }
    }
    list->items = realloc(list->items, ++count*sizeof(struct PStr));
    struct PStr *new_val = list->items + count - 1;
    new_val->capacity = -1;
    new_val->length = txt->length - secstart;
    new_val->text = txt->text + secstart;
    list->count = count;
    if (txt->capacity != -1) txt->capacity = -2;
    return list;
}

int CStr_equals_PStr(char *cstr, struct PStr *pstr) {
    if ((int)strlen(cstr) != pstr->length) return 0;
    return memcmp(cstr, pstr->text, pstr->length) == 0;
}

struct PStrPair *partition_PStr(struct PStr *txt, char *splitter) {
    int splitter_len = strlen(splitter);
    for (int i = 0; i <= txt->length - splitter_len; i++) {
        if (memcmp(txt->text + i, splitter, splitter_len) == 0) {
            struct PStrPair *pair = malloc(sizeof(struct PStrPair));
            struct PStr *first = &pair->first;
            first->capacity = -1;
            first->length = i;
            first->text = txt->text;
            struct PStr *second = &pair->second;
            second->capacity = -1;
            second->length = txt->length - i - splitter_len;
            second->text = txt->text + i + splitter_len;
            return pair;
        }
    }
    if (txt->capacity != -1) txt->capacity = -2;
    return NULL;
}

struct PStr *clone_PStr(struct PStr *old) {
    struct PStr *result = malloc(sizeof(struct PStr));
    result->capacity = old->length;
    result->length = old->length;
    result->text = malloc(old->length);
    memcpy(result->text, old->text, old->length);
    return result;
}

void copy_to_PStr(struct PStr *from, struct PStr *to) {
    to->capacity = -1;
    to->length = from->length;
    to->text = from->text;
}

void move_PStr(struct PStr *from, struct PStr *to) {
    to->capacity = from->capacity;
    to->length = from->length;
    to->text = from->text;
    from->capacity = -1;
    from->length = 0;
    from->text = NULL;
}

void CStr_copy_to_PStr(char *from, struct PStr *to) {
    to->capacity = -1;
    to->length = strlen(from);
    to->text = from;
}

void extend_PStr(struct PStr *str, const char *other, int other_len) {
    if (str->capacity < 0) {
        printf("Cannot extend dependent PStr\n");
        exit(1);
    }
    int new_len = str->length + other_len;
    if (str->capacity < new_len) {
        str->text = realloc(str->text, new_len);
        str->capacity = new_len;
    }
    memcpy(str->text + str->length, other, other_len);
    str->length = new_len;
}

struct PStr *join_PStrList(struct PStrList *list, char *sep, int sep_len) {
    if (list->count == 0) return new_PStr();
    int length = (list->count - 1) * sep_len;
    for (int i = 0; i < list->count; i++) {
        length += (list->items + i)->length;
    }
    struct PStr *str = malloc(sizeof(struct PStr));
    str->capacity = length;
    str->length = length;
    str->text = malloc(length);
    int pos = 0;
    for (int i = 0; i < list->count; i++) {
        if (i > 0) {
            memcpy(str->text + pos, sep, sep_len);
            pos += sep_len;
        }
        struct PStr *item = (list->items + i);
        memcpy(str->text + pos, item->text, item->length);
        pos += item->length;
    }
    return str;
}

struct PStr *_build_PStr(const char *fmt, va_list args) {
    struct PStr *result = new_PStr();

    int was_percent = 0;
    
    for (int i = 0; fmt[i]; i++) {
        if (was_percent) {
            was_percent = 0;
            if (fmt[i] == 's') {
                char *other = va_arg(args, char*);
                extend_PStr(result, other, strlen(other));
            } else if (fmt[i] == 'p') {
                struct PStr *other = va_arg(args, struct PStr*);
                extend_PStr(result, other->text, other->length);
            } else if (fmt[i] == 'l') {
                struct PStrList *list = va_arg(args, struct PStrList*);
                char *seperator = va_arg(args, char*);
                struct PStr *joined = join_PStrList(
                    list, seperator, strlen(seperator)
                );
                extend_PStr(result, joined->text, joined->length);
                free_PStr(joined);
            } else if (fmt[i] == '%') {
                extend_PStr(result, "%", 1);
            } else {
                printf("Invalid format specifier %%%c\n", fmt[i]);
                exit(1);
            }
            continue;
        } else {
            if (fmt[i] == '%') {
                was_percent = 1;
            } else {
                extend_PStr(result, fmt+i, 1);
            }
        }
    }

    return result;
}

struct PStr *build_PStr(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    
    struct PStr *result = _build_PStr(fmt, args);

    va_end(args);

    return result;
}

void printf_PStr(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    struct PStr *str = _build_PStr(fmt, args);

    va_end(args);

    print_PStr(str);
    free_PStr(str);
}

int recv_PStr(int sock, struct PStr *str) {
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
    int bytes_recv = recv(sock, str->text+old_len, MAX_RECV, 0);
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

int parse_enum(struct PStr *str, char *options[], int max) {
    for (int i = 0; i < max; i++) {
        if (CStr_equals_PStr(options[i], str)) {
            return i;
        }
    }
    return max;
}

enum HTTPMethod parse_method(struct PStr *str) {
    return parse_enum(str, METHODSTXT, UNKNOWNMETHOD);
}

enum HTTPVersion parse_http_version(struct PStr *str) {
    return parse_enum(str, VERSIONSTXT, UNKNOWNVERSION);
}

int parse_request_start_line(struct RequestHeaders *headers, struct PStr *start_line) {
    struct PStrList *parts = split_PStr(start_line, " ");
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
        printf("Invalid http header\n");
        return NULL;
    }
    return (struct Header*)pair;
}

struct Headers *parse_headers(int isRequest, struct PStr *txt) {
    struct Headers *headers = malloc(isRequest ? sizeof(struct RequestHeaders) : sizeof(struct ResponseHeaders));
    int headerCount = 0;
    struct Header **header_list = NULL;
    struct PStrList *list = split_PStr(txt, "\r\n");
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
        for (int j = 0; j < headerCount; j++) {
            struct PStr *listHeaderKey = &header_list[j]->key;
            int header_len = listHeaderKey->length;
            if (header_len != header->key.length) continue;
            if (memcmp(listHeaderKey->text, header->key.text, header_len) == 0) {
                printf("Same http header recieved twice\n");
                return NULL;
            }
        }
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
            break;
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

char *REQEND = "\r\n\r\n";

struct PStr *recv_headers(struct PStr *req, int remote) {
    int i = 0;
    while (1) {
        if (recv_PStr(remote, req)) {
            printf("Connection unexpectedly closed\n");
            return NULL;
        }
        int req_end_len = strlen(REQEND);
        int max_i = req->length + req_end_len - 1;
        for (; i < max_i; i++) {
            if (memcmp(req->text + i, REQEND, req_end_len) == 0) {
                return slice_PStr(req, 0, i);
            }
        }
    }
}

int recv_body(struct PStr *req, struct PStr *headersTxt, struct Headers *headers, int remote, struct PStr **request_body) {
    *request_body = new_PStr();
    struct PStr *transferEncoding = get_header(headers, "Transfer-Encoding");
    if (transferEncoding != NULL) {
        if (!CStr_equals_PStr("identity", transferEncoding)) {
            printf("Non-identity transfer encoding not supported\n");
            return 1;
        }
    }
    struct PStr *contentLengthTxt = get_header(headers, "Content-Length");
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
        if (recv_PStr(remote, req)) {
            return 1;
        }
    }
    *request_body = slice_PStr(req, ogLength, contentLength);
    return 0;
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

    if (send_PStr(target, request)) {
        printf("failed to forward request\n");
        exit(1);
    }

    struct PStr *res = new_PStr();
    struct PStr *response_headers = recv_headers(res, target);
    if (response_headers == NULL) {
        close(target);
        free_PStr(res);
        return NULL;
    }

    struct ResponseHeaders *headers = (struct ResponseHeaders*)parse_headers(RESPONSE, response_headers);
    if (headers == NULL) {
        close(target);
        free_PStr(res);
        free_PStr(response_headers);
        return NULL;
    }

    struct PStr *response_body;
    if (recv_body(res, response_headers, (struct Headers*)headers, target, &response_body)) {
        close(target);
        free_PStr(res);
        free_PStr(response_headers);
        free_ResponseHeaders(headers);
        return NULL;
    }

    struct PStr *redirect = get_header((struct Headers*)headers, "Location");
    if (redirect != NULL) {
        printf_PStr("%p\n", redirect);
    }

    struct PStr *new_headers = str_response_headers(headers);

    struct PStr *result = build_PStr(
        "%p%s%p", new_headers, REQEND, response_body
    );

    free_PStr(res);
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
    
    struct PStr *req = new_PStr();
    struct PStr *request_headers = recv_headers(req, remote);
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
    if (recv_body(req, request_headers, (struct Headers*)headers, remote, &request_body)) {
        close(remote);
        free_PStr(req);
        free_PStr(request_headers);
        free_RequestHeaders(headers);
        return;
    }

    remove_header((struct Headers*)headers, "Referer");
    
    set_header((struct Headers*)headers, "User-Agent", "Mozilla/5.0 (X11; CrOS x86_64 14541.0.0) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/121.0.0.0 Safari/537.36");
    
    struct PStr *target_host = build_PStr("%s:%s", TARGETHOSTNAME, TARGETPORT);
    set_header_PStr((struct Headers*)headers, "Host", target_host);
    
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
    int status;
    
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if ((status = getaddrinfo(TARGETHOSTNAME, TARGETPORT, &hints, &targetinfo))) {
        printf("getaddrinfo error: %s\n", gai_strerror(status));
        exit(1);
    }

    struct addrinfo *serverinfo;

    if ((status = getaddrinfo(NULL, SERVEPORT, &hints, &serverinfo))) {
        printf("getaddrinfo error: %s\n", gai_strerror(status));
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
