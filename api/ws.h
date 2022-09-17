#pragma once

#include "api/core.h"
#include "api/net.h"

#define MAX_NAME_LENGTH 32

typedef enum {
    WS_FLAG_PROC_MONITOR = (1 << 0),
} ws_flags_t;

typedef struct {
    ws_flags_t  flags;
    char        name_filter[MAX_NAME_LENGTH];
} ws_data_t;

typedef enum packed {
    WS_MSG_PROC_INFO = 1,
} ws_msg_t;

int ws_recv_commannd(session_t * const restrict session);
