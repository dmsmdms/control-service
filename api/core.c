#include "api/core.h"

#ifndef __USE_GNU
#define __USE_GNU
#endif

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <signal.h>
#include <stdio.h>
#include <errno.h>

#define MAX_LOG_SIZE    512
#define BASE64_PAD      '='

static const char base64en[] = {
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
    'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
    'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
    'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
    'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
    'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
    'w', 'x', 'y', 'z', '0', '1', '2', '3',
    '4', '5', '6', '7', '8', '9', '+', '/',
};

// https://github.com/zhicheng/base64/blob/master/base64.c
unsigned base64_encode(const uint8_t * const restrict in, const unsigned inlen, char * const restrict out) {
    unsigned s = 0;
    unsigned j = 0;
    uint8_t l = 0;

    for (unsigned i = 0; i < inlen; i++) {
        uint8_t c = in[i];

        switch (s) {
        case 0:
            s = 1;
            out[j++] = base64en[(c >> 2) & 0x3F];
            break;
        case 1:
            s = 2;
            out[j++] = base64en[((l & 0x3) << 4) | ((c >> 4) & 0xF)];
            break;
        case 2:
            s = 0;
            out[j++] = base64en[((l & 0xF) << 2) | ((c >> 6) & 0x3)];
            out[j++] = base64en[c & 0x3F];
            break;
        }
        l = c;
    }

    switch (s) {
    case 1:
        out[j++] = base64en[(l & 0x3) << 4];
        out[j++] = BASE64_PAD;
        out[j++] = BASE64_PAD;
        break;
    case 2:
        out[j++] = base64en[(l & 0xF) << 2];
        out[j++] = BASE64_PAD;
        break;
    }

    out[j] = 0;
    return j;
}

static void exit_handler(unused const int code) {
    exit(EXIT_SUCCESS);
}

int init_core(const char * const name) {
    openlog(name, NO_FLAGS, LOG_DEBUG);

    int result = atexit(closelog);
    return_if_system_error(result, NULL);

    result = fclose(stdin);
    return_if_system_error(result, NULL);
    stdin = NULL;

    result = fclose(stdout);
    return_if_system_error(result, NULL);
    stdout = NULL;

    result = fclose(stderr);
    return_if_system_error(result, NULL);
    stderr = NULL;

    sighandler_t sig_result = signal(SIGSEGV, exit_handler);
    return_if_signal_error(sig_result, NULL);

    sig_result = signal(SIGTERM, exit_handler);
    return_if_signal_error(sig_result, NULL);

    sig_result = signal(SIGPIPE, SIG_IGN);
    return_if_signal_error(sig_result, NULL);

    return EXIT_SUCCESS;
}

void _log(const char * const file, const unsigned line, const int log_level, const char * const msg, ...) {
    char buffer[MAX_LOG_SIZE], * restrict buf = buffer;
    buf += sprintf(buf, "%s:%u", file, line);

    if (errno != EXIT_SUCCESS) {
        *buf++ = ' ';
        buf += sprintf(buf, "errno - %s", strerror(errno));
    }

    if (msg != NULL) {
        va_list args;
        va_start(args, msg);

        *buf++ = ' ';
        buf += vsprintf(buf, msg, args);

        va_end(args);
    }

    *buf++ = '\n';
    *buf++ = '\0';

    syslog(log_level, "%s", buffer);
}
