#ifndef WIN_PLATFORM_CPP
#define WIN_PLATFORM_CPP

#include "platform.h"

#include <windows.h>
#include <stdint.h>

uint32_t get_current_thread_id() {
    return (uint32_t) GetCurrentThreadId();
}

#endif
