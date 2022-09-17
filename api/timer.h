#pragma once

#include "api/net.h"

extern struct timeval timer;

int handle_timers(session_t * restrict session);
