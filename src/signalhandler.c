
#include <stdbool.h>
#include <string.h>

#include <errno.h>
#include <signal.h>

#include "io.h"
#include "main.h"

void handle_signal(int signal)
{
    switch (signal) {
    case SIGINT:
    case SIGTERM:
        g_brexit = true; // exit cleanly
        break;

    case SIGHUP:
        g_reloadconfig = true; // reload config
        break;

    default:
        break;
    }
}

int init_signalhandler()
{
    struct sigaction action;

    action.sa_handler = handle_signal;
    action.sa_flags = 0;
    sigemptyset(&action.sa_mask);

    // register nonfatal handler
    if (sigaction(SIGHUP, &action, NULL)) {
        ERR("sigaction");
        return -1;
    }
    
    if (sigaction(SIGTERM, &action, NULL)) {
        ERR("sigaction");
        return -2;
    }
    
    if (sigaction(SIGINT, &action, NULL)) {
        ERR("sigaction");
        return -3;
    }

    // ignore sigchild
    action.sa_handler = SIG_IGN;
    if (sigaction(SIGCHLD, &action, NULL)) {
        ERR("sigaction (SIGCHLD)");
        return -4;
    }

    return 0;
}
