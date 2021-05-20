#include <inttypes.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <malloc.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "config.h"
#include "io.h"
#include "mbuffer.h"
#include "safe_lib.h"
#include "logger.h"

#define PREFIX long_int
#define T long int
#include "mbuffertemplate.h"

#define PREFIX size_t
#define T size_t
#include "mbuffertemplate.h"

#define PREFIX char
#define T char
#include "mbuffertemplate.h"

// config interpretation functions
static void cleaninput(char *input, const size_t size)
{
	size_t i, offset = 0;

	for (i = 0; i < size; i++) {
		const char curr = input[i - offset];

		// check for invalid characters
		if ((curr <= '9' && curr >= '0') ||
		    (curr <= 'Z' && curr >= 'A') ||
		    (curr <= 'z' && curr >= 'a') || curr == '/' ||
		    curr == ' ') {
			input[i - offset] = curr;

		} else if (curr == '\0' || curr == '\n') {
			input[i - offset] = '\0';

		} else {
			offset++;
		}
	}
	input[i] = '\0';
}

static long int parse_long(const char input[], char **end)
{
	long int num;
	errno = 0;

	num = strtol(input, end, 0);
	if (end != NULL) {
		if (*end == input) {
			return -2;
		}
	}

	// if the value is out of the range of a long
	if ((num == LLONG_MAX || num == LLONG_MIN) && errno == ERANGE) {
		return -3;
	}

	return num;
}

int readconfig(const char path[], struct configInfo *config)
{
	ssize_t rv;
	int fd;

	config->maxcount = -1;
	config->logpath[0] = '\0';
	config->xkeymaps = false;
	config->minavrg.tv_sec = 0;
	config->minavrg.tv_nsec = 0;

	// open the config file if it has no lock on it
	fd = open(path, O_RDWR | O_NOFOLLOW); // open the config
	if (fd < 0) {
		ERR(fd);
		return -1;
	}

	// try to lock the file if possible
	{
		struct flock lock;
		lock.l_type = F_WRLCK;
		lock.l_whence = SEEK_SET;

		lock.l_start = 0;
		lock.l_len = 0;

		rv = fcntl(fd, F_SETLK, &lock);
		if (rv) {
			if (errno == EACCES || errno == EAGAIN) {
				LOG(-1,
				    "Another instance is probably running!\n");
			}
			ERR(rv);
			return -2;
		}
	}

	{
		const size_t buffsize = 200;
		char buff[buffsize];

		lseek(fd, 0, SEEK_SET);
		while (rv > 0) {
			int i;

			memset_s(buff, buffsize, 0);
			rv = read(fd, &buff, buffsize);
			if (rv < 0) {
				ERR("read");
				return -3;
			}

			for (i = 0; i < rv; i++) {
				if (buff[i] == '\n' || buff[i] == '\0') {
					lseek(fd, i + 1 - rv, SEEK_CUR);

					// cleanup string
					if (i >= 0) {
						cleaninput(buff, (size_t)i);
					}
					LOG(2, "-> %s rv: %d\n", buff, rv);
					break;
				}
			}

			// gets the minimal average time difference between keystrokes
			if (strncmp(buff, "minavrg", 6) == 0) {
				char *end = NULL;

				config->minavrg.tv_sec =
					parse_long(&buff[8], &end);
				config->minavrg.tv_nsec =
					parse_long(&end[1], &end);

				if (config->minavrg.tv_sec < 0 ||
				    config->minavrg.tv_nsec < 0) {
					LOG(-1,
					    "The option of minavrg is malformed or out of range!\n");
					return -4;
				}

				LOG(1, "Minavrg set to %lds %ldns\n",
				    config->minavrg.tv_sec,
				    config->minavrg.tv_nsec);

			} else if (strncmp(buff, "maxscore", 7) == 0) {
				// sets the max score at which the device will be locked
				config->maxcount = parse_long(&buff[9], NULL);

				if (config->maxcount < 0) {
					LOG(-1,
					    "The option of maxscore is malformed or out of range!\n");
					return -5;
				}

				LOG(1, "Maxscore set to %ld\n",
				    config->maxcount);

			} else if (strncmp(buff, "logpath", 6) == 0) {
				struct stat st;

				// path where the logfile will be saved
				strcpy_s(config->logpath, PATH_MAX, &buff[8]);

				if (stat(config->logpath, &st) < 0) {
					if (errno == ENOENT) {
						if (mkdir(config->logpath,
							  731)) {
							ERR("mkdir");
							return -6;
						}
						LOG(0,
						    "Created logging directory!\n");

					} else {
						ERR("stat");
						return -7;
					}

				} else {
					if (S_ISDIR(st.st_mode)) {
						LOG(1,
						    "Set %s as the path for logging!\n",
						    config->logpath);

					} else {
						LOG(-1,
						    "Logpath does not point to a directory!\n");
						return -8;
					}
				}
			} else if (strncmp(buff, "usexkeymaps", 10) == 0) {
				// enables the use of x server keymaps if they are available
				if (parse_long(&buff[12], NULL) == 1) {
					config->xkeymaps = true;
					LOG(1, "UseXKeymaps set to 1!\n");
				}
			}
		}
	}
	if (close(fd)) {
		ERR("close");
	}
	return 0;
}

int handleargs(int argc, char *argv[], struct argInfo *data)
{
	int i;
	bool unrecognized = false;
	bool help = false;
	data->configpath[0] = '\0';

	for (i = 1; i < argc; i++) {
		if (argv[i][0] == '-') {
			switch (argv[i][1]) {
			// path to the config to be used
			case 'c':
				if (i + 1 < argc) {
					if (strcpy_s(data->configpath, PATH_MAX,
						     argv[i + 1]) != EOK) {
						return -1;
					}
				}
				break;

			// daemonize the daemon
			case 'd':
				g_daemonize = true;
				break;

			// increase the verbosity level
			case 'v':
				if (g_loglevel < MAX_LOGLEVEL) {
					g_loglevel++;
				} else {
					LOG(0,
					    "Can't increment loglevel any more!\n");
				}
				break;

			// shows help
			case 'h':
				LOG(0,
				    "duckydd %s\n"
				    "Usage: duckydd [Options]\n"
				    "\t\t-c <file>\tSpecify a config file path\n"
				    "\t\t-d\t\tDaemonize the process\n"
				    "\t\t-v\t\tIncrease verbosity of the console output (The maximum verbosity is 2)\n"
				    "\t\t\t\tTHE -v OPTION CAN POTENTIALY EXPOSE PASSWORDS!!!\n"
				    "\t\t-h\t\tShows this help section\n\n"
				    "For config options please have a look at the README.md\n"
				    "The daemon was linked against: udev "
#ifdef ENABLE_XKB_EXTENSION
				    "xkbcommon xkbcommon-x11 xcb "
#endif
				    "\n",
				    GIT_VERSION);
				help = true;
				break;

			default:
				LOG(0, "%s is not a recognized option. \n",
				    argv[i]);
				unrecognized = true;
				break;
			}
		}
	}

	if (unrecognized) {
		LOG(0,
		    "One or more options where not recognized! You can try the -h argument for a list of supported options.\n");
	}

	if (help) {
		return -2;
	} else if (data->configpath[0] == '\0') {
		LOG(0, "Please provide a config location!\n");
		return -3;
	}
	return 0;
}

// returns the filename from a path
const char *find_file(const char *input)
{
	size_t i;

	for (i = strnlen_s(input, PATH_MAX); i > 0;
	     i--) { // returns the filename
		if (input[i] == '/') {
			return &input[i + 1];
		}
	}
	return NULL;
}
