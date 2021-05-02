
#include <string.h>

#include <errno.h>

#include <signal.h>

#include "vars.h"
#include "logger.h"
#include "test_wrappers.h"

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
	ssize_t rv;
	struct sigaction action;

	action.sa_handler = handle_signal;
	action.sa_flags = 0;
	sigemptyset(&action.sa_mask);

	// register nonfatal handler
	rv = sigaction(SIGHUP, &action, NULL);
	if (rv) {
		ERR(rv);
		return -1;
	}

	rv = sigaction(SIGTERM, &action, NULL);
	if (rv) {
		ERR(rv);
		return -2;
	}

	rv = sigaction(SIGINT, &action, NULL);
	if (rv) {
		ERR(rv);
		return -3;
	}

	// ignore sigchild
	action.sa_handler = SIG_IGN;
	rv = sigaction(SIGCHLD, &action, NULL);
	if (rv) {
		ERR(rv);
		return -4;
	}
	return 0;
}
