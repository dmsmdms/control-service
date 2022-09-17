#include "api/http.h"
#include "api/core.h"
#include "api/ws.h"

#ifndef __USE_XOPEN_EXTENDED
#define __USE_XOPEN_EXTENDED
#endif

#include <openssl/sha.h>
#include <string.h>
#include <stdio.h>

#define HTTP_BUFFER_SIZE            (32 * 1024)
#define MAX_NAME_LENGHT             16
#define HTTP_GET_REQUEST            "GET"
#define HTTP_CODE_200               "200 OK"
#define HTTP_CODE_404               "404 Not Found"
#define HTTP_CONNECTION_CLOSE       "close"
#define HTTP_CONNECTION_KEEP_ALIVE  "keep-alive"
#define HTTP_CONTENT_TYPE_HTML      "text/html"
#define HTTP_CONTENT_TYPE_JS        "text/javascript"
#define HTTP_UPGRADE_WEB_SOCKET     "Upgrade: websocket"
#define HTTP_WEB_SOCKET_KEY         "Sec-WebSocket-Key:"
#define HTTP_WEB_SOCKET_GUID        "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"
#define HTTP_WEB_SOCKET_KEY_LEN     64
#define HTTP_MESSAGE_END            "\r\n"
#define HTTP_WEB_SOCKET_RESPONSE                            \
    "HTTP/1.1 101 Switching Protocols" HTTP_MESSAGE_END     \
    "Upgrade: websocket" HTTP_MESSAGE_END                   \
    "Connection: Upgrade" HTTP_MESSAGE_END                  \
    "Sec-WebSocket-Accept:"

typedef struct {
    const char          name[MAX_NAME_LENGHT];
    const void *        data;
    const unsigned *    size;
    const char *        type;
} ui_file_t;

extern const char build_main_js_ui_min[];
extern const char build_main_html_ui_min[];
extern const char build_not_found_html_ui_min[];
extern const unsigned build_main_js_ui_min_len;
extern const unsigned build_main_html_ui_min_len;
extern const unsigned build_not_found_html_ui_min_len;

static const ui_file_t ui_not_found = {
    "Not Found", build_not_found_html_ui_min, &build_not_found_html_ui_min_len, HTTP_CONTENT_TYPE_HTML
};

static const ui_file_t ui_files[] = {
    { "/", build_main_html_ui_min, &build_main_html_ui_min_len, HTTP_CONTENT_TYPE_HTML },
    { "/main.js", build_main_js_ui_min, &build_main_js_ui_min_len, HTTP_CONTENT_TYPE_JS },
};

static size_t make_header(char * buffer, const size_t size, const char * const code,
                            const char * const connection, const char * const type)
{
    size_t header_size = sprintf(buffer, "HTTP/1.1 %s" HTTP_MESSAGE_END, code);
    header_size += sprintf(buffer + header_size, "Connection: %s" HTTP_MESSAGE_END, connection);
    header_size += sprintf(buffer + header_size, "Content-type: %s" HTTP_MESSAGE_END, type);
    header_size += sprintf(buffer + header_size, "Content-length: %lu" HTTP_MESSAGE_END, size);
    strcpy(buffer + header_size, HTTP_MESSAGE_END);
    header_size += sizeof(HTTP_MESSAGE_END) - 1;
    return header_size;
}

static const ui_file_t * get_ui_file(const char * const name) {
    for (uint_fast8_t i = 0; i < array_len(ui_files); i++) {
        const ui_file_t * const restrict file = ui_files + i;

        if (strcmp(name, file->name) == EXIT_SUCCESS) {
            return file;
        }
    }

    return NULL;
}

static int http_send_ui_file(session_t * const restrict session) {
    const ui_file_t * restrict file = get_ui_file(session->data);
    const char * http_code = HTTP_CODE_200;
    char buffer[HTTP_BUFFER_SIZE];

    if (file == NULL) {
        http_code = HTTP_CODE_404;
        file = &ui_not_found;
    }

    const size_t size = make_header(buffer, *file->size, http_code, HTTP_CONNECTION_CLOSE, file->type);
    ssize_t result = session->write(session, buffer, size);
    return_if_system_error(result, NULL);

    result = session->write(session, file->data, *file->size);
    return_if_system_error(result, NULL);

    free(session->data);
    session->data = NULL;

    session->rd_callback = http_recv_commannd;
    session->wr_callback = NULL;

    return EXIT_SUCCESS;
}

static int http_upgrade(session_t * const restrict session) {
    uint8_t sha_code[SHA_DIGEST_LENGTH];
    SHA1(session->data, strlen(session->data), sha_code);

    char buffer[HTTP_BUFFER_SIZE] = HTTP_WEB_SOCKET_RESPONSE;
    size_t size = sizeof(HTTP_WEB_SOCKET_RESPONSE) - 1;
    buffer[size++] = ' ';

    size += base64_encode(sha_code, sizeof(sha_code), buffer + size);
    strcpy(buffer + size, HTTP_MESSAGE_END HTTP_MESSAGE_END);
    size += sizeof(HTTP_MESSAGE_END HTTP_MESSAGE_END) - 1;

    ssize_t result = session->write(session, buffer, size);
    return_if_system_error(result, NULL);

    free(session->data);
    session->data = NULL;

    session->rd_callback = ws_recv_commannd;
    session->wr_callback = NULL;

    return EXIT_SUCCESS;
}

static int get_request_handler(session_t * const restrict session, char * const buffer) {
    if (strstr(buffer, HTTP_UPGRADE_WEB_SOCKET) != NULL) {
        char * begin = strstr(buffer, HTTP_WEB_SOCKET_KEY);
        return_if_memory_error(begin, NULL);
        begin += sizeof(HTTP_WEB_SOCKET_KEY);

        char * restrict end = strchr(begin, '\r');
        if (end == NULL) {
            end = strchr(begin, '\n');
        }
        return_if_memory_error(end, NULL);
        *end = '\0';

        if (session->data != NULL) {
            free(session->data);
        }

        const size_t length = end - begin + sizeof(HTTP_WEB_SOCKET_GUID);
        session->data = malloc(length);

        char * data = session->data;
        strcpy(data, begin);

        data += (end - begin);
        strcpy(data, HTTP_WEB_SOCKET_GUID);

        session->wr_callback = http_upgrade;
        session->rd_callback = NULL;
    } else {
        char * const restrict end = strchr(buffer, ' ');
        return_if_memory_error(end, NULL);
        *end = '\0';

        if (session->data != NULL) {
            free(session->data);
        }

        session->data = strdup(buffer);
        return_if_memory_error(session->data, NULL);

        session->wr_callback = http_send_ui_file;
        session->rd_callback = NULL;
    }

    return EXIT_SUCCESS;
}

int http_recv_commannd(session_t * const restrict session) {
    char buffer[HTTP_BUFFER_SIZE];

    ssize_t result = session->read(session, buffer, sizeof(buffer) - 1);
    return_if_system_error(result, NULL);
    return_if_cond(result == 0, NULL);

    buffer[result] = '\0';

    if (memcmp(buffer, HTTP_GET_REQUEST, sizeof(HTTP_GET_REQUEST) - 1) == EXIT_SUCCESS) {
        return get_request_handler(session, buffer + sizeof(HTTP_GET_REQUEST));
    }

    return EXIT_SUCCESS;
}
