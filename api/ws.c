#include "api/proc.h"
#include "api/ws.h"

#ifndef __USE_POSIX
#define __USE_POSIX
#endif

#include <netinet/in.h>
#include <signal.h>
#include <string.h>

#define WS_BUFFER_SIZE              (64 * 1024)
#define WS_HEAD_SIZE                2
#define WS_LENGTH_SIZE              2
#define WS_MASK_SIZE                4
#define WS_LENGTH_LOW_LIMIT         126
#define WS_LENGTH_MID_LIMIT         127
#define WS_FRAME_FULL_BIT           0x80
#define WS_BASE_SEND_OFFSET         (WS_HEAD_SIZE + WS_LENGTH_SIZE)
#define IS_WS_FRAME_FULL(buffer)    (buffer[0] & WS_FRAME_FULL_BIT)
#define HAS_WS_MASK(buffer)         (buffer[1] & 0x80)
#define GET_WS_TYPE(buffer)         (buffer[0] & 0x0F)
#define GET_WS_LOW_LEN(buffer)      (buffer[1] & 0x7F)
#define GET_WS_MID_LEN(buffer)      *(uint16_t *)&buffer[2]

typedef enum {
    WS_TYPE_TEXT = 0x1,
    WS_TYPE_BIN = 0x2,
    WS_TYPE_CLOSE = 0x8,
    WS_TYPE_PING = 0x9,
    WS_TYPE_PONG = 0xA,
} ws_type_t;

typedef enum packed {
    WS_CMD_BIND_PROC_MONITOR = 1,
    WS_CMD_UNBIND_PROC_MONITOR = 2,
    WS_CMD_GET_PROC_INFO = 3,
    WS_CMD_CONTINUE_PROC = 4,
    WS_CMD_PAUSE_PROC = 5,
    WS_CMD_STOP_PROC = 6,
    WS_CMD_KILL_PROC = 7,
} ws_cmd_t;

static int ws_send_proc_info(session_t * const restrict session) {
    uint8_t buffer[WS_BUFFER_SIZE];
    uint8_t * restrict buf = buffer + WS_BASE_SEND_OFFSET;
    const ws_data_t * const restrict data = session->data;

    const char * const name = (data->name_filter[0] != '\0' ? data->name_filter : NULL);
    ssize_t size = get_proc_by_name(name, buf);
    return_if_system_error(size, NULL);

    if (size >= WS_LENGTH_LOW_LIMIT) {
        buf -= sizeof(uint16_t);
        *(uint16_t *)buf = htons(size);

        buf -= sizeof(uint8_t);
        *(uint8_t *)buf = WS_LENGTH_LOW_LIMIT;
    } else {
        buf -= sizeof(uint8_t);
        *(uint8_t *)buf = size;
    }

    buf -= sizeof(uint8_t);
    *(uint8_t *)buf = WS_FRAME_FULL_BIT | WS_TYPE_BIN;
    size += (buffer - buf) + WS_BASE_SEND_OFFSET;

    size = session->write(session, buf, size);
    return_if_system_error(size, NULL);

    session->wr_callback = NULL;
    return EXIT_SUCCESS;
}

static int ws_timer_handler(session_t * const restrict session) {
    const ws_data_t * const restrict data = session->data;

    if (data->flags & WS_FLAG_PROC_MONITOR) {
        session->wr_callback = ws_send_proc_info;
    }

    return EXIT_SUCCESS;
}

static int ws_cmd_handler(session_t * const restrict session, char * restrict buffer, const uint_fast16_t size) {
    ws_data_t * restrict data = session->data;

    if (data == NULL) {
        data = malloc(sizeof(ws_data_t));
        data->flags = NO_FLAGS;
        data->name_filter[0] = '\0';
        session->data = data;
    }

    char * const end = buffer + size;
    while (buffer < end) {
        const ws_cmd_t cmd = *buffer++;

        switch (cmd) {
        case WS_CMD_GET_PROC_INFO: {
            const uint8_t length = *buffer++;
            memcpy(data->name_filter, buffer, length);
            buffer += length;

            session->wr_callback = ws_send_proc_info;
            data->name_filter[length] = '\0';
        } break;
        case WS_CMD_CONTINUE_PROC: {
            const pid_t pid = *(uint32_t *)buffer;
            buffer += sizeof(uint32_t);

            const int result = kill(pid, SIGCONT);
            return_if_system_error(result, NULL);
        } break;
        case WS_CMD_PAUSE_PROC: {
            const pid_t pid = *(uint32_t *)buffer;
            buffer += sizeof(uint32_t);

            const int result = kill(pid, SIGSTOP);
            return_if_system_error(result, NULL);
        } break;
        case WS_CMD_STOP_PROC: {
            const pid_t pid = *(uint32_t *)buffer;
            buffer += sizeof(uint32_t);

            const int result = kill(pid, SIGTERM);
            return_if_system_error(result, NULL);
        } break;
        case WS_CMD_KILL_PROC: {
            const pid_t pid = *(uint32_t *)buffer;
            buffer += sizeof(uint32_t);

            const int result = kill(pid, SIGKILL);
            return_if_system_error(result, NULL);
        } break;
        case WS_CMD_BIND_PROC_MONITOR:
            data->flags |= WS_FLAG_PROC_MONITOR;
            session->timer_callback = ws_timer_handler;
            break;
        case WS_CMD_UNBIND_PROC_MONITOR:
            data->flags &= ~(WS_FLAG_PROC_MONITOR);
            session->timer_callback = NULL;
            break;
        }
    }

    return EXIT_SUCCESS;
}

int ws_recv_commannd(session_t * const restrict session) {
    char buffer[WS_BUFFER_SIZE], * restrict buf = buffer;

    ssize_t result = session->read(session, buffer, sizeof(buffer) - 1);
    return_if_system_error(result, NULL);
    return_if_cond(result == 0, NULL);

    if (IS_WS_FRAME_FULL(buffer) == false) {
        return EXIT_SUCCESS;
    }

    const ws_type_t type = GET_WS_TYPE(buffer);
    return_if_cond(type == WS_TYPE_CLOSE, NULL);

    if (type != WS_TYPE_BIN) {
        return EXIT_SUCCESS;
    }

    uint_fast16_t length = GET_WS_LOW_LEN(buffer);
    return_if_cond(length == WS_LENGTH_MID_LIMIT, NULL);
    buf += WS_HEAD_SIZE;

    if (length == WS_LENGTH_LOW_LIMIT) {
        length = GET_WS_MID_LEN(buffer);
        length = ntohs(length);
        buf += WS_LENGTH_SIZE;
    }

    uint8_t * const restrict mask = (void *)buf;
    buf += WS_MASK_SIZE;

    result -= (buf - buffer);
    return_if_cond(result != (ssize_t)length, NULL);

    for (uint_fast16_t i = 0; i < length; i++) {
        buf[i] ^= mask[i % WS_MASK_SIZE];
    }

    return ws_cmd_handler(session, buf, length);
}
