//
// Copyright (c) 2023 vorakl
// SPDX-License-Identifier: Apache-2.0
//

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
#include <sys/sendfile.h>
#include <sys/resource.h>
#include <sys/mman.h>
#include "common.hpp"
#include "httpserver.hpp"


int get_tcp_listener(sa_family_t af,
                     in_addr_t hostaddr,
                     in_port_t port) {
    int srv_sock;
    sockaddr_in srv_addr{}; // initialize with all zeros!!!
    const int on = 1;

    // Define IP protocol, Address, and Port
    srv_addr.sin_family = af;
    srv_addr.sin_addr.s_addr = htonl(hostaddr);
    srv_addr.sin_port = htons(port);

    // Create a socket for accepting TCP connections
    if ((srv_sock = socket(af, SOCK_STREAM, 0)) == -1) {
        handle_error("get_tcp_listener::socket");
    }

    // Set socket options
    if (setsockopt(srv_sock, SOL_SOCKET, SO_REUSEADDR, &on,
                   sizeof(on)) == -1) {
        handle_error("get_tcp_listener::setsockopt");
    }

    // Attach a socket to a local network interface
    if (bind(srv_sock, (sockaddr *) &srv_addr,
             sizeof(sockaddr)) == -1) {
        handle_error("get_tcp_listener::bind");
    }

    // begin accepting connections on the socket
    if (listen(srv_sock, BACKLOG_CONN) == -1) {
        handle_error("get_tcp_listener::listen");
    }

    return srv_sock;
}


int accept_tcp_conn(int srv_sock, sockaddr_in* peer_addr) {
    int peer_sock;
    socklen_t addr_len = sizeof(sockaddr);

    if ((peer_sock = accept(srv_sock,
                            (sockaddr*) peer_addr, &addr_len)) == -1) {
    	if (errno != EINTR) {
    		handle_error("accept_tcp_connection::accept");
    	}
    }

    return peer_sock;
}


void* serve_in_thread(void* arg) {
    connInfo* ci = (connInfo*) arg;
    http_req_t http_req{};
    std::string req_header;
    char msg[MSG_SIZE]{};

    req_header = get_req_header(ci);
    if (req_header == "") {
        http_response_400(ci); // There is no a header
        goto finish;
    } else {
        strncpy(msg, req_header.c_str(), MSG_SIZE-1);
    }

    if (parse_http_req(&http_req, msg) == -1) {
        http_response_400(ci); // There is no a header
        goto finish;
    } else {
        process_http_req(ci, &http_req);
    }

finish:
    close(ci->sock);  // Close the TCP connection!
    delete ci;
    ci = nullptr;

    return nullptr;
}


int parse_http_req(http_req_t* req, char* str) {
    // str format: '<verb><space><uri><space>HTTP/<ver>\n'
    char* token;
    char  tmp[VER_SIZE]{};
    char  buf[BUF_SIZE]{};
    char  buf2[BUF_SIZE]{};

    // get verb
    token = strtok(str, " \t\n\r");
    if (token != nullptr) {
    	memset(req->verb, 0, VERB_SIZE);
        strncpy(req->verb, token, VERB_SIZE-1);
    } else {
        return -1;
    }

    // get uri
    token = strtok(nullptr, " \t\n\r");
    if (token != nullptr) {
    	memset(req->uri, 0, URI_SIZE);
        strncpy(req->uri, token, URI_SIZE-1);
    } else {
        return -1;
    }

    // get HTTP version
    token = strtok(nullptr, " \t\n\r");
    if (token != nullptr) {
        strncpy(tmp, token, VER_SIZE-1);

        token = strtok(tmp, "/");
        token = strtok(nullptr, "/");
        if (token != nullptr) {
        	memset(req->ver, 0, VER_SIZE);
            strcpy(req->ver, token);
        } else {
            return -1;
        }
    } else {
    	return -1;
    }

    // extract QUERY STRING (a part after ?)
    memset(buf, 0, BUF_SIZE);
    memset(buf2, 0, BUF_SIZE);
    strncpy(buf2, req->uri, BUF_SIZE-1);
    token = strtok(buf2, "?");
    if (token != nullptr) {
        // before ? symbol
        strncpy(buf, token, BUF_SIZE-1);

        // get QUERY_STRING
        token = strtok(nullptr, "?");
        if (token != nullptr) {
            strncpy(req->query_str, token, QUERYSTR_SIZE-1);
        } else {
            memset(req->query_str, 0, QUERYSTR_SIZE);
        }
    } else {
        strncpy(buf, req->uri, BUF_SIZE-1);
    }

    // Extract full file name
    std::string s1(buf);
    std::regex rerep ("^https?://");
    s1 = std::regex_replace(s1, rerep, ""); // remove https?://

    std::regex re1("^[^/]*(/.*)$");
    std::smatch m1;
    if (std::regex_match(s1, m1, re1)) {
    	strncpy(req->fpath, m1[1].str().c_str(), FNAME_SIZE-1);
    }

    // Extract an extension
    memset(buf, 0, BUF_SIZE);
    memset(req->ext, 0, EXT_SIZE);
    strncpy(buf, req->fpath, BUF_SIZE-1);

    token = strtok(buf, ".");
    while (token != nullptr) {
        strncpy(req->ext, token, EXT_SIZE-1);
        token = strtok(nullptr, ".");
    }

    return 0;
}


void http_response_400(const connInfo* ci) {
    char buf[BUF_SIZE]{};

    sprintf(buf, "400 Bad request");
    log(ci->pipeWr, ci->mutex, buf);

    sprintf(buf, "HTTP/1.1 400 Bad request\r\nConnection: Close\r\nContent-Length: 0\r\n\r\n");
    write(ci->sock, buf, strlen(buf));
}


void http_response_404(const connInfo* ci, const http_req_t* req) {
    char msg[MSG_SIZE]{};

    sprintf(msg, "%s %s 404 File not found", req->verb, req->uri);
    log(ci->pipeWr, ci->mutex, msg);

    memset(msg, 0, MSG_SIZE);
    sprintf(msg, "HTTP/%s 404 File not found\r\nConnection: Close\r\nContent-Length: 0\r\n\r\n",
            req->ver);
    write(ci->sock, msg, strlen(msg));
}


void http_response_500(const connInfo* ci, const http_req_t* req) {
    char msg[MSG_SIZE]{};

    sprintf(msg, "%s %s 500 Internal Server Error", req->verb, req->uri);
    log(ci->pipeWr, ci->mutex, msg);

    memset(msg, 0, MSG_SIZE);
    sprintf(msg, "HTTP/%s 500 Internal Server Error\r\nConnection: Close\r\nContent-Length: 0\r\n\r\n",
    		req->ver);
    write(ci->sock, msg, strlen(msg));
}


void http_response_quit(const connInfo* ci, const http_req_t* req) {
    char msg[MSG_SIZE]{};
    const char* shutdownMsg = "Received a QUIT request. Shutting Down...";

    sprintf(msg, "%s %s 200 OK content-type: text/plain content-length: %li",
            req->verb, req->uri, strlen(shutdownMsg));
    log(ci->pipeWr, ci->mutex, msg);
    log(ci->pipeWr, ci->mutex, shutdownMsg);

    memset(msg, 0, MSG_SIZE);
    sprintf(msg, "HTTP/1.1 200 OK\r\nConnection: Close\r\nContent-Type: text/plain\r\nContent-Length: %li\r\n\r\n%s",
            strlen(shutdownMsg), shutdownMsg);
    write(ci->sock, msg, strlen(msg));
}


void process_http_req(const connInfo* ci, const http_req_t* req) {
    if (strcmp(req->fpath, "/quit") == 0) {
        // tell the main process to stop gracefully and exit
        http_response_quit(ci, req);
        kill(getpid(), SIGTERM);
    } else {
        http_try_files(ci, req); // Look for files in the DocumentRoot
    }
}


void http_try_files(const connInfo* ci, const http_req_t* req) {
    char fn[FNAME_SIZE]{};
    char fn_send[FNAME_SIZE*2]{};
    char ex[EXT_SIZE]{};
    size_t fs = 0;
    char ft[FTYPE_SIZE]{};
    char msg[MSG_SIZE]{};
    bool type_found = false;
    struct stat file_stat;
    int fd;
    int bsent = 0;
    off_t offset = 0;
    int bleft;


	// Check for the DirectoryIndex
	if (req->fpath[strlen(req->fpath)-1] == '/') {
		strcpy(fn, req->fpath);
		strncat(fn, DIRECTORY_INDEX, FNAME_SIZE-1);
		strcpy(ex, "html");
	} else {
		strcpy(fn, req->fpath);
		strcpy(ex, req->ext);
	}

	// Look up a file
	strcpy(fn_send, ci->doc_root);
	strncat(fn_send, fn, FNAME_SIZE);
restart_syscall:
	fd = open(fn_send, O_RDONLY);
	if (fd == -1) {
		if (errno == EINTR) {
			goto restart_syscall;
		}
		// file not found
		http_response_404(ci, req);
	    goto finish;
	} else {
	    if (fstat(fd, &file_stat) == -1) {
	    	// unable to get info about a file
	    	http_response_500(ci, req);
		    goto finish;
	    }
		fs = file_stat.st_size;
	}

	// Search for a file type
	for (int i=0; i <= FTYPES_COUNT; ++i) {
		if (strcmp(ex, file_types[i].ext) == 0) {
			strcpy(ft, file_types[i].type);
			type_found = true;
		}
	}
	if (! type_found) {
		strcpy(ft, "application/octet-stream");
	}

	// send logs
	memset(msg, 0, MSG_SIZE);
	sprintf(msg, "%s %s 200 OK content-type: %s content-length: %li",
			req->verb, req->uri, ft, fs);
	log(ci->pipeWr, ci->mutex, msg);

	// send a header
	memset(msg, 0, MSG_SIZE);
	sprintf(msg, "HTTP/%s 200 OK\r\nConnection: Close\r\nContent-Type: %s\r\nContent-Length: %li\r\n\r\n",
			req->ver, ft, fs);
	write(ci->sock, msg, strlen(msg));

	// send a file
	bleft = fs;
	while (((bsent = sendfile(ci->sock, fd, &offset, BUFSIZ)) > 0) && (bleft > 0)) {
			bleft -= bsent;
	}

finish:
	return;
}


std::string get_req_header(const connInfo* ci) {
	bool 	exit_loop = false;
    ssize_t cnt;  // a number of read bytes
	char	buf[BUF_SIZE]{};
	char	buf2[BUF_SIZE]{};
	std::vector<std::string> req_data;

	// Reading from the socket
    while (! exit_loop) {
        cnt = read(ci->sock, buf, BUF_SIZE-1);
        if (cnt == -1) {
            perror("serve_in_thread::read");
        } else if (cnt == 0) {
            exit_loop = true; // TCP client disconnected
        } else {
			buf[cnt] = '\0'; // make a string from a byte array
			memset(buf2, 0, BUF_SIZE);
			for (int i = 0, k = 0; buf[i] != '\0'; ++i) {
				if (buf[i] == '\r' && buf[i+1] == '\n') {
					// check in a line ends with \r\n
					if (strlen(buf2) == 0) {
						 // Header has finished. Exit and go processing it
						 exit_loop = true;
						 break;
					}
					req_data.push_back(std::string(buf2));
					k = 0;
					++i;
					memset(buf2, 0, BUF_SIZE);
					continue;
				} else if (buf[i] == '\n') {
					// check in a line ends with \n
					if (strlen(buf2) == 0) {
						 // Header has finished. Exit and go processing it
						 exit_loop = true;
						 break;
					}
					req_data.push_back(std::string(buf2));
					k = 0;
					memset(buf2, 0, BUF_SIZE);
					continue;
				}
				buf2[k++] = buf[i];
			}
			if (strlen(buf2) != 0) {
				req_data.push_back(std::string(buf2));
			}
        }
    }

	if (req_data.size() >= 1) {
	    return req_data[0]; // Get only 1st line of a header for parsing
	} else {
		return "";
	}
}

