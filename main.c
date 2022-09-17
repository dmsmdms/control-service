#include "api/config.h"
#include "api/timer.h"
#include "api/core.h"

// Can be changed from another file.
bool app_is_running = true;

int main(const int argc, char ** const argv) {
    int result = init_core(argv[0]);
    return_if_system_error(result, NULL);

    result = init_config(argc, argv);
    return_if_system_error(result, NULL);

    result = init_socket(&master_sock, config.port);
    return_if_system_error(result, NULL);

    result = atexit(free_all_sessions);
    return_if_system_error(result, NULL);

    while (app_is_running != false) {
        FD_ZERO(&rd_set);
        FD_ZERO(&wr_set);

        struct timeval * restrict timeval = NULL;
        FD_SET(master_sock, &rd_set);

        for (session_t * restrict session = sessions; session != NULL; session = session->next) {
            if (session->rd_callback != NULL) {
                FD_SET(session->socket, &rd_set);
            }

            if (session->wr_callback != NULL) {
                FD_SET(session->socket, &wr_set);
            }

            if (session->timer_callback != NULL) {
                timeval = &timer;
            }
        }

        result = select(max_fd + 1, &rd_set, &wr_set, NULL, timeval);
        return_if_system_error(result, NULL);

        if (timeval != NULL) {
            result = handle_timers(sessions);
            return_if_system_error(result, NULL);
        }

        result = make_session(master_sock);
        return_if_system_error(result, NULL);

        for (session_t * restrict session = sessions, * next; session != NULL; session = next) {
            next = session->next;

            if (FD_ISSET(session->socket, &rd_set)) {
                result = session->rd_callback(session);
                goto_if_system_error(result, free_session, NULL);
            }

            if (FD_ISSET(session->socket, &wr_set)) {
                result = session->wr_callback(session);
                goto_if_system_error(result, free_session, NULL);
            }

            continue;
        free_session:
            result = free_session(session);
            return_if_system_error(result, NULL);
        }
    }

    return EXIT_SUCCESS;
}
