//
// Copyright (c) 2023 vorakl
// SPDX-License-Identifier: Apache-2.0
//

#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include <unistd.h>
#include <pthread.h>
#include <sysexits.h>
#include <signal.h>
#include "common.hpp"
#include "httpserver.hpp"


#define Wr 1
#define Rd 0

const char* shutdownMsg = "Shutting Down.";
bool DONE = false;

struct loggerInfo {
    int         pipeRd;
    pthread_t   thread;
};


void* runLogger(void* arg);
void term_handler(int sig); // signal handler


int main() {
    int             pipefds[2];
    loggerInfo      li{};
    int             res;
    pthread_mutex_t mut;
    int             srv_sock;
    sockaddr_in     peer_addr{}; // initialize with all zeros!!!
    connInfo*       ci;
    char            msg[64]{};
    struct sigaction term_sa{};


    // Create Pipe
    if ((res = pipe(pipefds)) != 0) {
        handle_error_en(res, "main::pipe");
    }

    // Init Mutex
    pthread_mutex_init(&mut, nullptr);

    // Init Signal Action and assign a handler for SIGTERM
    sigemptyset(&term_sa.sa_mask);
    term_sa.sa_flags = 0; // Disable SA_RESTART: DO NOT restart accept()
    term_sa.sa_handler = term_handler;
    sigaction(SIGTERM, &term_sa, NULL);
    sigaction(SIGINT, &term_sa, NULL);

    // Launch the Logger
    li.pipeRd = pipefds[Rd];
    res = pthread_create(&li.thread, nullptr, &runLogger, &li);
    if (res != 0) {
        handle_error_en(res, "main::pthread_create::logger");
    }

    // Launch a TCP server
    srv_sock = get_tcp_listener(AF_INET, INADDR_ANY, LISTEN_PORT);
    sprintf(msg, "Listening on port %d", LISTEN_PORT);
    log(pipefds[Wr], &mut, msg);

    // Main loop
    do {
        ci = new connInfo; // delete it in a thread !!!
        ci->sock = accept_tcp_conn(srv_sock, &peer_addr);

        if (ci->sock == -1) {
            // Got EINTR
            if (DONE) {
                break; // got SIGTERM and set DONE, exiting...
            }
            continue; // restart accept() after any signal other than SIGTERM
        }

        // fill out the structure as an argument for a new thread
        ci->addr = &peer_addr;
        ci->mutex = &mut;
        ci->pipeWr = pipefds[Wr];

        memset(ci->doc_root, 0, FNAME_SIZE);
        if (getenv("HTTP_DOC_ROOT") != nullptr &&
            strlen(getenv("HTTP_DOC_ROOT")) != 0) {
            // DOC_ROOT is defined in the Env
            strncpy(ci->doc_root, getenv("HTTP_DOC_ROOT"), FNAME_SIZE-1);
        } else {
            strncpy(ci->doc_root, DOCUMENT_ROOT, FNAME_SIZE-1);
        }

        // Create a thread to process an accepted connection
        res = pthread_create(&(ci->thread), nullptr, &serve_in_thread, (void *) ci);
        if (res != 0) {
            handle_error_en(res, "main::pthread_create::serve_in_thread");
        }
    } while(!DONE);

    // Shutting down...
    sleep(1); // wait a bit for threads finishing their tasks

    // close Pipe's write fd to initiate closing the Logger
    close(pipefds[Wr]);
    res = pthread_join(li.thread, nullptr);
    if (res != 0) {
        handle_error_en(res, "main::pthread_join::logger");
    }
    close(pipefds[Rd]);

    res = pthread_mutex_destroy(&mut);
    if (res != 0) {
        handle_error_en(res, "main::pthread_mutex_destroy");
    }

    // close the listening socket
    close(srv_sock);

    printf("Server terminated.\n");

    return EX_OK;
}

void* runLogger(void* arg) {
    char buf[BUF_SIZE];
    int i = 0;
    ssize_t cnt;
    auto li = (loggerInfo*) arg;

    printf("Logger pid: %d tid: %d\n", getpid(), gettid());

    do {
        // reading by 1 character
        cnt = read(li->pipeRd, &buf[i], 1);
        if (cnt > 0) {
            // check if it's the end of a string
            if (buf[i] == '\0') {
                i = 0;
                printf("%s\n", buf);
            } else
                i++;
        } else if (cnt == 0) {
            break; // write end of a pipe was closed
        } else {
            handle_error("logger");
        }
    } while (true);  // loops until received all write FDs are closed

    return nullptr;
}

void term_handler(int sig) {
    DONE = true;   // Set exit flag on receiving the TERM signal
}
