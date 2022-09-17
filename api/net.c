#include "api/http.h"
#include "api/core.h"

#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdlib.h>

session_t * sessions = NULL;
fd_set rd_set = { false };
fd_set wr_set = { false };
int master_sock = INVALID_FD;
int max_fd = INVALID_FD;

int init_socket(int * const restrict socket_ptr, const uint16_t port) {
    const int socket_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    return_if_system_error(socket_fd, NULL);
    *socket_ptr = socket_fd;

    if (max_fd < socket_fd) {
        max_fd = socket_fd;
    }

    struct sockaddr_in sockaddr = {
        .sin_family = AF_INET,
        .sin_port = htons(port),
        .sin_addr.s_addr = htonl(INADDR_ANY),
    };

    int result = bind(socket_fd, (struct sockaddr *)&sockaddr, sizeof(sockaddr));
    return_if_system_error(result, NULL);

    const int reuse_addr = true;
    result = setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &reuse_addr, sizeof(reuse_addr));
    return_if_system_error(result, NULL);

    result = listen(socket_fd, SOMAXCONN);
    return_if_system_error(socket_fd, NULL);

    return EXIT_SUCCESS;
}

static ssize_t read_callback(const session_t * const restrict session, void * const buffer, const size_t size) {
    return read(session->socket, buffer, size);
}

static ssize_t write_callback(const session_t * const restrict session, const void * const buffer, const size_t size) {
    return write(session->socket, buffer, size);
}

int make_session(const int socket) {
    if (FD_ISSET(socket, &rd_set)) {
        session_t * const new_session = malloc(sizeof(session_t));
        return_if_memory_error(new_session, NULL);

        new_session->next = sessions;
        sessions = new_session;

        new_session->rd_callback = http_recv_commannd;
        new_session->wr_callback = NULL;
        new_session->timer_callback = NULL;
        new_session->read = read_callback;
        new_session->write = write_callback;
        new_session->socket = INVALID_FD;
        new_session->data = NULL;

        const int client_fd = accept(socket, NULL, NULL);
        return_if_system_error(client_fd, NULL);
        new_session->socket = client_fd;

        if (max_fd < client_fd) {
            max_fd = client_fd;
        }
    }

    return EXIT_SUCCESS;
}

static int real_free_session(session_t * const restrict session) {
    if (session->socket >= MIN_VALID_FD) {
        const int result = close(session->socket);
        return_if_system_error(result, NULL);
    }

    if (session->data != NULL) {
        free(session->data);
    }

    if (session->socket == max_fd) {
        max_fd--;
    }

    free(session);
    return EXIT_SUCCESS;
}

int free_session(session_t * const restrict session) {
    session_t ** restrict last_session = &sessions;

    for (session_t * restrict cur_session = sessions; cur_session != NULL; cur_session = cur_session->next) {
        if (cur_session == session) {
            *last_session = cur_session->next;
            return real_free_session(session);
        }

        last_session = &cur_session->next;
    }

    return INVALID_RESULT;
}

void free_all_sessions(void) {
    if (master_sock >= MIN_VALID_FD) {
        close(master_sock);
        master_sock = INVALID_FD;
    }

    for (session_t * restrict session = sessions, * next; session != NULL; session = next) {
        next = session->next;
        real_free_session(session);
    }

    sessions = NULL;
    max_fd = INVALID_FD;
}
