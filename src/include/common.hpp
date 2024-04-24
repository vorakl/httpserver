//
// Copyright (c) 2023 vorakl
// SPDX-License-Identifier: Apache-2.0
//

#ifndef SRC_COMMON_H_
#define SRC_COMMON_H_

#include <sysexits.h>
#include <pthread.h>
#include <cstring>
#include <unistd.h>


#define handle_error_en(en, msg) \
               do { errno = en; perror(msg); exit(EX_OSERR); } while (0)
#define handle_error(msg) \
               do { perror(msg); exit(EX_OSERR); } while (0)


void log(int fd, pthread_mutex_t* mutex, const char* buf);

#endif /* SRC_COMMON_H_ */
