httpserver
==========

A pretty small (a dynamically linked binary is under 200KB), very basic
(supports GET requests), web server that serves only a static content over HTTP.
However, it is multi-threaded, quite fast and does the job.
I wrote it for fun to help me learn the basics of network programming.
The main goal was to get it to the point where it's fully capable of serving my static
[personal website](https://vorakl.com/), and currently it fully meets that
requirement.


Design
------
The `main()` creates a logger thread and then waits in a loop for new connections
via `accept_tcp_conn()`. The logger receives log messages from worker threads through a pipe and prints them to standard output. The main thread, as soon as
receives a new connection, it will create a new thread, pass the connection to it, and then immediately go back to waiting for another connection. The main thread is able to handle the TERM signal for graceful termination. Worker threads serve files relative to a document root defined in the `HTTP_DOC_ROOT` environment variable. As an example, there is an endpoint `/quit` with a custom handler `http_response_quit()` that also terminates the server.

How to build and run
--------------------

It's supposed to be built and run on Linux OS because of the use of a non-POSIX,
Linux flavored `sendfile()` and `gettid()`.
Just run `make` or `make build` in the `src` directory.
By default, it uses GNU C++. To build with CLang, run `make CXX=clang++`.

If it's built successfully, run a binary `./bin/httpserver`. By default,
it serves `/var/www/`, but you can define another document root by setting
up the environment variable `HTTP_DOC_ROOT`.

Example:

```
$ cd src

$ make
/usr/lib64/ccache/g++ -std=c++17 -O2 -Wall -Iinclude -c -o obj/common.o common.cpp
/usr/lib64/ccache/g++ -std=c++17 -O2 -Wall -Iinclude -c -o obj/httpserver.o httpserver.cpp
/usr/lib64/ccache/g++ -std=c++17 -O2 -Wall -Iinclude -c -o obj/main.o main.cpp
/usr/lib64/ccache/g++ -std=c++17 -O2 -Wall -Iinclude -o bin/httpserver obj/common.o obj/httpserver.o obj/main.o

$ env HTTP_DOC_ROOT=/var/www/http ./bin/httpserver
Logger pid: 1824702 tid: 1824703
Listening on port 8080
```
or 

```
$ cd src

$ make clean
rm -f obj/*.o *~ core include/*~ bin/httpserver

$ make CXX=clang++
clang++ -std=c++17 -O2 -Wall -Iinclude -c -o obj/common.o common.cpp
clang++ -std=c++17 -O2 -Wall -Iinclude -c -o obj/httpserver.o httpserver.cpp
clang++ -std=c++17 -O2 -Wall -Iinclude -c -o obj/main.o main.cpp
clang++ -std=c++17 -O2 -Wall -Iinclude -o bin/httpserver obj/common.o obj/httpserver.o obj/main.o

$ env HTTP_DOC_ROOT=/var/www/http ./bin/httpserver
Logger pid: 1827739 tid: 1827740
Listening on port 8080
```

In the last example, `1827739` is the PID of the main thread, and `1827740` is
the thread ID of the logger:

```
$ pidstat -p 1827739 -t
Linux 6.5.12-100.fc37.x86_64 (sirius) 	04/23/2024 	_x86_64_	(12 CPU)

08:40:43 PM   UID      TGID       TID    %usr %system  %guest   %wait    %CPU   CPU  Command
08:40:43 PM  1000   1827739         -    0.00    0.00    0.00    0.00    0.00     2  httpserver
08:40:43 PM  1000         -   1827739    0.00    0.00    0.00    0.00    0.00     2  |__httpserver
08:40:43 PM  1000         -   1827740    0.00    0.00    0.00    0.00    0.00     8  |__httpserver
```

To stop the server, there are a few options:

* send the `TERM` signal to the main thread, e.g. `pkill httpserver`
* send the `INT` signal, e.g. press CTRL+C on the control terminal
* send `/quit` request, e.g. `curl http://localhost:8080/quit`


Have fun!
---------
Don't expect too much from it, but it can be a good starting point.

