#include "api/config.h"
#include "api/core.h"

#include <stdlib.h>
#include <getopt.h>

#define PORT_DEFAULT 8080

typedef enum {
    OPT_CODE_END = -1,
    OPT_CODE_PORT = 'p',
    OPT_CODE_UNDEFINED = '?',
} opt_code_t;

static const struct option longopts[] = {
    { "port", required_argument, NULL, OPT_CODE_PORT },
    { 0 },
};

config_t config = {
    .port = PORT_DEFAULT,
};

int init_config(const int argc, char ** const argv) {
    while (true) {
        const opt_code_t code = getopt_long(argc, argv, "p:", longopts, NULL);

        switch (code) {
        case OPT_CODE_END:
            return EXIT_SUCCESS;
        case OPT_CODE_PORT:
            config.port = atoi(optarg);
            break;
        case OPT_CODE_UNDEFINED:
            return INVALID_RESULT;
        }
    }
}
