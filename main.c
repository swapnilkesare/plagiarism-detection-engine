#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

#include "backend/backend.h"
#include "frontend/frontend.h"

static void handle_sigint(int sig) {
    (void)sig;
    printf("\n[SHUTDOWN] Stopping Plagiarism Detection Engine...\n");
    frontend_stop();
}

int main(int argc, char *argv[]) {
    int port = 8080;
    if (argc > 1) {
        port = atoi(argv[1]);
        if (port <= 0 || port > 65535) port = 8080;
    }

    /* Register signal handler for graceful shutdown */
    signal(SIGINT, handle_sigint);
    signal(SIGTERM, handle_sigint);

    /* 1. Initialize Backend Subsystem */
    backend_init();

    /* 2. Initialize Frontend HTTP Server */
    if (!frontend_init(port)) {
        fprintf(stderr, "[ERROR] Could not start server on port %d.\n", port);
        backend_cleanup();
        return 1;
    }

    printf("========================================================\n");
    printf("   Plagiarism Detection Engine - SLA Stage 2\n");
    printf("   Pure C Architecture | Arrays & Binary Search Trees\n");
    printf("========================================================\n");
    printf("Server started successfully.\n");
    printf("Open: http://127.0.0.1:%d\n", port);
    printf("Press Ctrl+C to stop server.\n");
    printf("========================================================\n");

    /* 3. Run Server Event Loop */
    frontend_run();

    /* 4. Cleanup Subsystems */
    frontend_cleanup();
    backend_cleanup();

    printf("[SHUTDOWN] Server stopped cleanly.\n");
    return 0;
}
