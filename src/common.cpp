//
// Copyright (c) 2023 vorakl
// SPDX-License-Identifier: Apache-2.0
//

#include <sysexits.h>
#include <pthread.h>
#include <cstring>
#include <unistd.h>
#include "common.hpp"


void log(int fd, pthread_mutex_t* mutex, const char* buf) {
    pthread_mutex_lock(mutex);
    write(fd, buf, strlen(buf)+1);
    pthread_mutex_unlock(mutex);
}
