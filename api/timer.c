#include "api/timer.h"
#include "api/core.h"

#ifndef __USE_MISC
#define __USE_MISC
#endif

#include <sys/time.h>
#include <stdlib.h>

#define TIMER_REFRESH_PERIOD_SEC    (1)
#define TIMER_REFRESH_PERIOD_USEC   (0)

struct timeval timer = {
    .tv_sec = TIMER_REFRESH_PERIOD_SEC,
    .tv_usec = TIMER_REFRESH_PERIOD_USEC,
};

int handle_timers(session_t * restrict session) {
    if (timerisset(&timer) == false) {
        while (session != NULL) {
            if (session->timer_callback != NULL) {
                const int result = session->timer_callback(session);
                return_if_system_error(result, NULL);
            }

            session = session->next;
        }

        timer.tv_sec = TIMER_REFRESH_PERIOD_SEC;
        timer.tv_usec = TIMER_REFRESH_PERIOD_USEC;
    }

    return EXIT_SUCCESS;
}
