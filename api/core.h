#pragma once

#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <syslog.h>

#define constructor     __attribute__((constructor))
#define noreturn        __attribute__((noreturn))
#define packed          __attribute__((packed))
#define unused          __attribute__((unused))
#define array_len(arr)  (sizeof(arr) / sizeof(arr[0]))

#define MAX_PATH_LENGTH     512
#define NO_FLAGS            0
#define MIN_VALID_RESULT    0
#define MIN_VALID_FD        0
#define INVALID_RESULT      -1
#define INVALID_FD          -1

#define return_if_system_error(result, msg, ...)                \
    if ((int)result < 0) {                                      \
        _log(__FILE__, __LINE__, LOG_ERR, msg, ##__VA_ARGS__);  \
        return result;                                          \
    } (void)(result)

#define return_if_signal_error(result, msg, ...)                \
    if (result == SIG_ERR) {                                    \
        _log(__FILE__, __LINE__, LOG_ERR, msg, ##__VA_ARGS__);  \
        return INVALID_RESULT;                                  \
    } (void)(result)

#define return_if_cond(cond, msg, ...)                          \
    if (cond) {                                                 \
        _log(__FILE__, __LINE__, LOG_ERR, msg, ##__VA_ARGS__);  \
        return INVALID_RESULT;                                  \
    } (void)(cond)

#define return_if_memory_error(ptr, msg, ...)                   \
    if (ptr == NULL) {                                          \
        _log(__FILE__, __LINE__, LOG_ERR, msg, ##__VA_ARGS__);  \
        return INVALID_RESULT;                                  \
    } (void)(ptr)

#define goto_if_system_error(result, point, msg, ...)           \
    if ((int)result < 0) {                                      \
        _log(__FILE__, __LINE__, LOG_ERR, msg, ##__VA_ARGS__);  \
        goto point;                                             \
    } (void)(result)

#define log(msg, ...) \
    _log(__FILE__, __LINE__, LOG_ERR, msg, ##__VA_ARGS__)

extern bool app_is_running;

int init_core(const char * const name);
void _log(const char * const file, const unsigned line, const int log_level, const char * const msg, ...);
unsigned base64_encode(const uint8_t * const restrict in, const unsigned inlen, char * const restrict out);
