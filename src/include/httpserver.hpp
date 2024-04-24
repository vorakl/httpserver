//
// Copyright (c) 2023 vorakl
// SPDX-License-Identifier: Apache-2.0
//

#ifndef HTTPSERVER_H
#define HTTPSERVER_H

#include <cstring>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <string>
#include <sstream>
#include <iostream>
#include <regex>
#include <fcntl.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <sys/mman.h>
#include "common.hpp"

#define BUF_SIZE 2048
#define MSG_SIZE 2048
#define LISTEN_PORT 8080
#define VERB_SIZE 10
#define VER_SIZE 10
#define FNAME_SIZE 640
#define FTYPE_SIZE 64
#define EXT_SIZE 10
#define QUERYSTR_SIZE 128
#define URI_SIZE 1024
#define BACKLOG_CONN 128   // a number of pending connections
#define DOCUMENT_ROOT "/var/www"
#define DIRECTORY_INDEX "index.html"
#define FTYPES_COUNT 11


struct connInfo {
    pthread_t          thread;
    int                sock;
    const sockaddr_in* addr;
    int                pipeWr;
    pthread_mutex_t*   mutex;
    char               doc_root[FNAME_SIZE];
};

struct http_req_t {
    char verb[VERB_SIZE];
    char ver[VER_SIZE];
    char uri[URI_SIZE];
    char fpath[FNAME_SIZE];
    char ext[EXT_SIZE];
    char query_str[QUERYSTR_SIZE];
};

struct file_type_t {
    char ext[EXT_SIZE];
    char type[FTYPE_SIZE];
};

const file_type_t file_types[FTYPES_COUNT] = {
    {"html", "text/html"},
	{"htm", "text/html"},
	{"txt", "text/plain"},
	{"png", "image/png"},
	{"css", "text/css"},
	{"js", "text/javascript"},
	{"ico", "image/vnd.microsoft.icon"},
	{"jpg", "image/jpeg"},
	{"jpeg", "image/jpeg"},
	{"gif", "image/gif"},
	{"xml", "application/xml"}
};

extern char** environ;

int get_tcp_listener(sa_family_t af, in_addr_t hostaddr, in_port_t port);
int accept_tcp_conn(int srv_sock, sockaddr_in* peer_addr);
void* serve_in_thread(void* arg);
std::string get_req_header(const connInfo* ci);
int parse_http_req(http_req_t* req, char* str);
void process_http_req(const connInfo* ci, const http_req_t* req);
void http_response_400(const connInfo* ci);
void http_response_404(const connInfo* ci, const http_req_t* req);
void http_response_500(const connInfo* ci, const http_req_t* req);
void http_response_quit(const connInfo* ci, const http_req_t* req);
void http_try_files(const connInfo* ci, const http_req_t* req);

#endif // HTTPSERVER_H

