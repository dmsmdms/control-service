#pragma once

#include <stdint.h>

typedef struct {
    uint16_t port;
} config_t;

extern config_t config;

int init_config(const int argc, char ** const argv);
