#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <time.h>
#include <stdatomic.h>
#include <pthread.h>
#include "config.h"
#include "http.h"
#include "pstr.h"
#include "tls.h"

struct TargetInfo {
    atomic_int rc;
    struct Origin *target_origin;
    struct addrinfo *targetaddr;
};

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
struct TargetInfo *locked_target_info = NULL;

struct ThreadData {
    void *payload;
    void (*func)(void *data);
    atomic_bool is_running;
};

struct ThreadData threads_data[MAX_THREAD_COUNT];

bool set_target(struct Origin *origin);

void decr_TargetInfo_rc(struct TargetInfo *info) {
    if (info == NULL) return;
    if (--info->rc <= 0) {
        free_Origin(info->target_origin);
        freeaddrinfo(info->targetaddr);
        free(info);
    }
}

void incr_TargetInfo_rc(struct TargetInfo *info) {
    if (info != NULL) info->rc++;
}

int get_free_thread_idx() {
    for (int i = 0; i < MAX_THREAD_COUNT; i++) {
        if (!threads_data[i].is_running) {
            return i;
        }
    }
    return -1;
}

void thread_cleanup(void *data_ptr) {
    ((struct ThreadData*)data_ptr)->is_running = false;
}

void *thread_entry(void *data_ptr) {
    pthread_cleanup_push(&thread_cleanup, data_ptr);
    struct ThreadData *data = (struct ThreadData*)data_ptr;
    (*data->func)(data->payload);
    pthread_cleanup_pop(1);
    return NULL;
}

int start_thread(void (*func)(void *payload), void *payload) {
    int idx = get_free_thread_idx();
    if (idx == -1) return -1;
    struct ThreadData *thread_data = &threads_data[idx];
    thread_data->payload = payload;
    thread_data->func = func;
    thread_data->is_running = true;
    pthread_t thread;
    pthread_attr_t thread_attr;
    pthread_attr_init(&thread_attr);
    pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_DETACHED);
    int creation_status = pthread_create(&thread, &thread_attr, &thread_entry, thread_data);
    pthread_attr_destroy(&thread_attr);
    if (creation_status) {
        return -1;
    }
    return idx;
}

struct ResponseHeaders *make_empty_ResponseHeaders(int status) {
    struct ResponseHeaders *headers = malloc(sizeof(*headers));
    
    headers->http_version = HTTP11;
    headers->status = PStr_from_int_len(status, 3);

    headers->count = 0;
    headers->headers = NULL;

    return headers;
}

void make_HTTP_date(char buffer[64]) {
    time_t now;
    time(&now);
    struct tm *timeinfo = gmtime(&now);
    strftime(buffer, 63, "%a, %d %b %Y %T %Z", timeinfo);
}

struct PStr *render_template(char *path, int pairCount, char *keys[], struct PStr *vals[]) {
    struct PStr *result = read_file(path);
    for (int i = 0; i < pairCount; i++) {
        char *key = keys[i];
        int key_len = strlen(key);
        int from_len = key_len + 2;
        char *from = malloc(from_len);
        from[0] = '{';
        memcpy(from + 1, key, key_len);
        from[from_len - 1] = '}';
        struct PStr *to = vals[i];
        PStr_replace_inline(result, from, from_len, to->text, to->length);
    }
    return result;
}

bool send_special_response(int remote, struct PStr *response) {
    int send_status = send_PStr(remote, response);
    free_PStr(response);
    if (send_status) {
        printf("Failed to send special response\n");
        return true;
    }

    return false;
}

bool serve_content(int remote, int status, char *content, int contentLength, enum ContentType contentType) {
    struct ResponseHeaders *responseHeaders = make_empty_ResponseHeaders(status);

    char date[64];
    make_HTTP_date(date);

    char *contentLengthStr = CStr_from_int(contentLength);
    add_headers((struct Headers*)responseHeaders, 4, (char*[]) {
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
        contentLengthStr
    });
    free(contentLengthStr);

    struct PStr contentPStr = {-1, contentLength, content};

    struct PStr *responseHeadersTxt = str_response_headers(responseHeaders);
    struct PStr *response = build_PStr("%p"HEADERSEND"%p", responseHeadersTxt, &contentPStr);

    free_PStr(responseHeadersTxt);
    free_ResponseHeaders(responseHeaders);

    return send_special_response(remote, response);
}

bool serve_file(int remote, struct RequestHeaders *request, int status, char *path, enum ContentType contentType) {
    struct PStr *str = read_file(path);

    bool success = serve_content(remote, status, str->text, str->length, contentType);

    free_PStr(str);

    return success;
}

bool serve_redirect(int remote, struct RequestHeaders *headers, int status, char *url) {
    struct ResponseHeaders *responseHeaders = make_empty_ResponseHeaders(status);

    char date[64];
    make_HTTP_date(date);

    add_headers((struct Headers*)responseHeaders, 4, (char*[]) {
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

    struct PStr *responseHeadersTxt = str_response_headers(responseHeaders);
    struct PStr *response = build_PStr("%p"HEADERSEND, responseHeadersTxt);

    free_PStr(responseHeadersTxt);
    free_ResponseHeaders(responseHeaders);

    return send_special_response(remote, response);
}

bool serve_empty(int remote, int status) {
    return serve_content(remote, status, "", 0, TEXTPLAIN_CONTENTTYPE);
}

bool serve_forward_redirect(int remote, struct PStr *target) {
    struct PStr *rendered = render_template("web/redirect.html", 1, (char*[]) {
        "url"
    }, (struct PStr*[]) {
        target
    });
    bool success = serve_content(remote, 200, rendered->text, rendered->length, TEXTHTML_CONTENTTYPE);
    free_PStr(rendered);
    return success;
}

bool serve_request(int remote, struct RequestHeaders *headers, struct PStr *body) {
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
        if (origin == NULL) {
            return serve_empty(remote, 403);
        }
        if (set_target(origin)) {
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

/*
unsigned int AHH[] = {
    6174, 11777, 7115, 11498, 6248, 6415, 6317, 18272, 8072, 11884, 6252, 8465, 7957
};
*/

bool set_target(struct Origin *origin) {
    /*
    unsigned int HH = dumb_hash(origin->hostname, strlen(origin->hostname));
    bool isA = false;
    unsigned long AHHCount = sizeof(AHH) / sizeof(AHH[0]);
    for (unsigned char i = 0; i < (unsigned char)AHHCount; i++) {
        if (HH == AHH[i]) {
            isA = true;
            break;
        }
    }
    if (!isA) return true;
    */
    
    struct TargetInfo *target_info = malloc(sizeof(*target_info));
    target_info->rc = 1;
    target_info->target_origin = origin;

    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    int getaddrinfo_status = getaddrinfo(
        origin->hostname, get_origin_port(origin), 
        &hints, &target_info->targetaddr
    );
    if (getaddrinfo_status) {
        printf("getaddrinfo error: %s\n", gai_strerror(getaddrinfo_status));
        exit(1);
    }

    pthread_mutex_lock(&lock);
    decr_TargetInfo_rc(locked_target_info);
    locked_target_info = target_info;
    pthread_mutex_unlock(&lock);

    return false;
}

void handle_forwarding(int remote, struct PStr *request, struct TargetInfo *target_info) {
    struct addrinfo *targetaddr = target_info->targetaddr;
    struct Origin *target_origin = target_info->target_origin;
    
    int target = socket(targetaddr->ai_family, 
    targetaddr->ai_socktype, targetaddr->ai_protocol);
    if (target == -1) {
        printf("target = socket() error\n");
        exit(1);
    }

    if (connect(target, targetaddr->ai_addr, targetaddr->ai_addrlen)) {
        printf("connect to target error\n");
        exit(1);
    }

    SSL *ssl = NULL;
    recv_PStr recver;

    if (uses_SSL(target_origin->protocol)) {
        ssl = upgrade_to_SSL(target_origin->hostname, target);

        if (ssl == NULL) return;

        if (send_PStr_SSL(ssl, request)) {
            printf("failed to forward request through SSL\n");
            return;
        }

        set_SSL_recv(ssl);
        recver = &recv_PStr_SSL;
    } else {
        if (send_PStr(target, request)) {
            printf("failed to forward request\n");
            return;
        }

        set_basic_recv_sock(target);
        recver = &recv_PStr_basic;
    }

    struct PStr *res1 = new_PStr();
    struct PStr *response_headers = recv_headers(res1, recver);
    if (response_headers == NULL) {
        close(target);
        free_PStr(res1);
        return;
    }

    struct ResponseHeaders *headers = (struct ResponseHeaders*)parse_headers(RESPONSE, response_headers);
    if (headers == NULL) {
        close(target);
        free_PStr(res1);
        free_PStr(response_headers);
        return;
    }

    if (headers->http_version > HTTP11) {
        close(target);
        free_PStr(res1);
        free_PStr(response_headers);
        free_ResponseHeaders(headers);
        printf("Only http version 1.1 is supported for responses\n");
        return;
    }

    struct PStr *res2 = clone_PStr(res1);

    struct PStr *response_body;
    if (recv_body(res2, response_headers, (struct Headers*)headers, recver, &response_body)) {
        close(target);
        free_PStr(res1);
        free_PStr(res2);
        free_PStr(response_headers);
        free_ResponseHeaders(headers);
        return;
    }

    set_header((struct Headers*)headers, "cache-control", "no-store");
    remove_header((struct Headers*)headers, "expires");

    remove_header((struct Headers*)headers, "strict-transport-security");
    remove_header((struct Headers*)headers, "content-security-policy");
    // backwards compatibility:
    remove_header((struct Headers*)headers, "x-content-security-policy");
    remove_header((struct Headers*)headers, "x-frame-options");

    struct PStr *redirect = get_header((struct Headers*)headers, "location");

    if (redirect == NULL) {
        struct PStr *new_headers = str_response_headers(headers);
        struct PStr *response = build_PStr(
            "%p"HEADERSEND"%p", new_headers, response_body
        );
        free_PStr(new_headers);

        if (send_PStr(remote, response)) {
            printf("Failed to send response to client\n");
        }
        free_PStr(response);
    } else {
        serve_forward_redirect(remote, redirect);
    }

    free_PStr(res1);
    free_PStr(res2);
    free_PStr(response_headers);
    free_ResponseHeaders(headers);
    free_PStr(response_body);
    if (ssl != NULL) close_SSL(ssl);
    close(target);
}

void read_forwarding_request(int remote, struct RequestHeaders *headers, struct PStr *request_body, struct TargetInfo *target_info) {
    remove_header((struct Headers*)headers, "referer");

    set_header((struct Headers*)headers, "user-agent", "Mozilla/5.0 (X11; CrOS x86_64 14541.0.0) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/121.0.0.0 Safari/537.36");

    set_header((struct Headers*)headers, "accept-encoding", "identity");

    struct Origin *target_origin = target_info->target_origin;
    struct PStr *target_host = build_PStr(
        "%s:%s", target_origin->hostname, get_origin_port(target_origin)
    );
    set_header_PStr((struct Headers*)headers, "host", target_host);

    struct PStr *new_headers = str_request_headers(headers);

    struct PStr *forwardee = build_PStr(
        "%p"HEADERSEND"%p", new_headers, request_body
    );
    free_PStr(new_headers);

    handle_forwarding(
        remote, forwardee, target_info
    );

    free_PStr(forwardee);
}

void handle_request(void *remote_storage) {
    int remote = *(int*)remote_storage;
    free(remote_storage);

    pthread_mutex_lock(&lock);
    struct TargetInfo *target_info = locked_target_info;
    incr_TargetInfo_rc(target_info);
    pthread_mutex_unlock(&lock);

    set_basic_recv_sock(remote);
    recv_PStr recver = &recv_PStr_basic;

    struct PStr *req1 = new_PStr();
    struct PStr *request_headers = recv_headers(req1, recver);
    if (request_headers == NULL) {
        goto cleanup_req1;
    }

    struct RequestHeaders *headers = (struct RequestHeaders*)parse_headers(REQUEST, request_headers);
    if (headers == NULL) {
        goto cleanup_str_headers;
    }

    if (headers->http_version > HTTP11) {
        printf("Only http version 1.1 is supported for requests\n");
        goto cleanup_headers;
    }

    struct PStr *req2 = clone_PStr(req1);
    struct PStr *request_body;
    if (recv_body(req2, request_headers, (struct Headers*)headers, recver, &request_body)) {
        goto cleanup_req2;
    }

    if (PStr_starts_with(headers->url, SPECIALURL, strlen(SPECIALURL))) {
        serve_request(remote, headers, request_body);
    } else if (target_info == NULL) {
        serve_redirect(remote, headers, 307, SPECIALURL VIEWURL);
    } else {
        read_forwarding_request(remote, headers, request_body, target_info);
    }

// cleanup_body:
    free_PStr(request_body);
cleanup_req2:
    free_PStr(req2);
cleanup_headers:
    free_RequestHeaders(headers);
cleanup_str_headers:
    free_PStr(request_headers);
cleanup_req1:
    free_PStr(req1);
    close(remote);
    decr_TargetInfo_rc(target_info);
}

void server_loop(int server) {
    while (1) {
        struct sockaddr_storage remote_addr;
        socklen_t remote_addr_size = sizeof(remote_addr);
        int remote = accept(server, (struct sockaddr*)&remote_addr, &remote_addr_size);

        int *remote_storage = malloc(sizeof(*remote_storage));
        *remote_storage = remote;

        int thread_idx = start_thread(&handle_request, remote_storage);
        if (thread_idx == -1) {
            // We can't create a thread for some reason, do this operation
            // on the main thread instead
            handle_request(remote_storage);
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

    server_loop(server);

    shutdown_SSL();
    
    pthread_mutex_lock(&lock);
    decr_TargetInfo_rc(locked_target_info);
    pthread_mutex_unlock(&lock);

    return 0;
}
