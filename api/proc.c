#include "api/proc.h"
#include "api/ws.h"

#ifndef __USE_MISC
#define __USE_MISC
#endif

#include <netinet/in.h>
#include <dirent.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>

#define MAX_PROC_COUNT  128
#define PROC_DIR_PATH   "/proc"
#define PROC_NAME_FILE  "comm"

typedef struct packed {
    uint32_t    pid;
    uint32_t    virt_mem;
    uint32_t    phy_mem;
    uint32_t    sh_mem;
    uint32_t    time;
    uint16_t    cpu;
    uint16_t    mem;
    uint8_t     status;
    uint8_t     name_len;
    char        name[];
} proc_info_t;

static DIR * dir = NULL;
static int file_fd = INVALID_FD;

static bool is_digit_str(const char * restrict str) {
    while (*str != '\0') {
        if (isdigit(*str++) == false) {
            return false;
        }
    }

    return true;
}

ssize_t get_proc_by_name(const char * const name, uint8_t * const buffer) {
    uint8_t * restrict buf = buffer;
    *buf++ = WS_MSG_PROC_INFO;

    uint8_t * const restrict length_ptr = buf;
    uint_fast8_t length = 0;
    buf++;

    dir = opendir(PROC_DIR_PATH);
    return_if_memory_error(dir, NULL);

    while (length < MAX_PROC_COUNT) {
        struct dirent * entry = readdir(dir);

        if (entry == NULL) {
            break;
        }

        if (entry->d_type != DT_DIR || is_digit_str(entry->d_name) == false) {
            continue;
        }

        char proc_name[MAX_PATH_LENGTH];
        sprintf(proc_name, PROC_DIR_PATH "/%s/" PROC_NAME_FILE, entry->d_name);

        file_fd = open(proc_name, O_RDONLY);
        return_if_system_error(file_fd, NULL);

        int result = read(file_fd, proc_name, sizeof(proc_name));
        return_if_system_error(result, NULL);
        proc_name[result] = '\0';

        result = close(file_fd);
        return_if_system_error(result, NULL);

        if (name == NULL || strstr(proc_name, name) != NULL) {
            proc_info_t * const restrict info = (void *)buf;
            const pid_t pid = atoi(entry->d_name);

            info->pid = htonl(pid);
            info->virt_mem = 0;
            info->phy_mem = 0;
            info->sh_mem = 0;
            info->time = 0;
            info->cpu = 0;
            info->mem = 0;
            info->status = 0;
            info->name_len = strlen(proc_name);
            strcpy(info->name, proc_name);

            buf += sizeof(proc_info_t) + info->name_len;
            length++;
        }
    }

    closedir(dir);
    dir = NULL;

    *length_ptr = length;
    return buf - buffer;
}
