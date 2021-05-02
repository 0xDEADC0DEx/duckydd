#include <stdio.h>
#include <stdlib.h>

#include <sys/stat.h>
#include <unistd.h>

#include "io.h"
#include "safe_lib.h"
#include "test_wrappers.h"

int become_daemon(struct configInfo config)
{
	ssize_t rv;

	// fork so that the child is not the process group leader
	rv = fork();
	if (rv != 0) {
		return -1;
	}

	// become a process group and session leader
	if (setsid() == -1) {
		return -2;
	}

	// fork so the child cant regain control of the controlling terminal
	rv = fork();
	if (rv != 0) {
		return -3;
	}

	// change base dir
	if (chdir("/")) {
		return -4;
	}

	// reset file mode creation mask
	umask(0);

	// close std file descriptors
	if (fclose(stdin)) {
		return -5;
	}

	{
		char path[PATH_MAX] = { '\0' };

		strcpy_s(path, PATH_MAX, config.logpath);
		strcat_s(path, PATH_MAX, "/out.log");

		// log to a file
		if (freopen(path, "w", stdout) != stdout) {
			return -6;
		}

		if (freopen(path, "w", stderr) != stderr) {
			return -7;
		}
	}
	return 0;
}
