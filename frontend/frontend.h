#ifndef FRONTEND_H
#define FRONTEND_H

#include <stdbool.h>

/* Initializes HTTP server socket on loopback 127.0.0.1 and specified port */
bool frontend_init(int port);

/* Main server loop (listening and serving requests) */
void frontend_run(void);

/* Signals the server to stop gracefully */
void frontend_stop(void);

/* Frees sockets and Winsock runtime resources */
void frontend_cleanup(void);

#endif /* FRONTEND_H */
