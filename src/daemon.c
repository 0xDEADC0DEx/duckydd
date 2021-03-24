#include <stdio.h>
#include <stdlib.h>

#include <sys/stat.h>
#include <unistd.h>

#include "io.h"
#include "safe_lib.h"
#include "test_wrappers.h"

int become_daemon(struct configInfo config)
{
	int rv;

	// fork so that the child is not the process group leader
	rv = fork();
	if (rv != 0) {
		return 1;
	}

	// become a process group and session leader
	if (setsid() == -1) {
		return 1;
	}

	// fork so the child cant regain control of the controlling terminal
	rv = fork();
	if (rv != 0) {
		return 1;
	}

	// change base dir
	if (chdir("/")) {
		return 1;
	}

	// reset file mode creation mask
	umask(0);

	// close std file descriptors
	if (fclose(stdout)) {
		return 1;
	}

	if (fclose(stdin)) {
		return 1;
	}

	if (fclose(stderr)) {
		return 1;
	}

	{
		char path[PATH_MAX] = { '\0' };

		pathcpy(path, config.logpath);
		pathcat(path, "/out.log");

		// log to a file
		if (freopen(path, "w", stdout) != stdout) {
			return 1;
		}

		if (freopen(path, "w", stderr) != stderr) {
			return 1;
		}
	}
	return 0;
}
