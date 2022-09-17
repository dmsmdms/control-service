#pragma once

#include <sys/select.h>
#include <unistd.h>
#include <stdint.h>

typedef struct session session_t;
typedef int (* session_cb_t)(session_t * const restrict session);
typedef ssize_t (* read_cb_t)(const session_t * const restrict session, void * const buffer, const size_t size);
typedef ssize_t (* write_cb_t)(const session_t * const restrict session, const void * const buffer, const size_t size);

typedef struct session {
    session_t *     next;
    session_cb_t    rd_callback;
    session_cb_t    wr_callback;
    session_cb_t    timer_callback;
    read_cb_t       read;
    write_cb_t      write;
    void            * data;
    int             socket;
} session_t;

extern session_t * sessions;
extern fd_set rd_set;
extern fd_set wr_set;
extern int master_sock;
extern int max_fd;

int init_socket(int * const restrict socket_ptr, const uint16_t port);
int make_session(const int socket);
int free_session(session_t * const restrict session);
void free_all_sessions(void);
