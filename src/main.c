/*
 * This is a PoC that implements some ideas which should help protect against
 * rubber ducky attacks as a daemon
 * */

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <sys/epoll.h>
#include <sys/time.h>
#include <sys/resource.h>

#include "daemon.h"
#include "io.h"
#include "logkeys.h"
#include "mbuffer.h"
#include "safe_lib.h"
#include "signalhandler.h"
#include "udev.h"
#include "device.h"
#include "test_wrappers.h"
#include "logger.h"

#if _POSIX_TIMERS <= 0
#error "Can't use posix time functions!"
#endif

#define PREFIX deviceInfo
#define T struct deviceInfo
#include "mbuffertemplate.h"

// global variables
bool g_brexit;
bool g_reloadconfig;
bool g_daemonize;
short g_loglevel;

int init(char configpath[], struct udevInfo *udev, struct configInfo *config,
	 struct keyboardInfo *kbd, struct managedBuffer *device, int *epollfd,
	 struct epoll_event *udevevent)
{
	ssize_t rv;

	// reset global variables
	g_brexit = false;
	g_reloadconfig = false;

	// init device managed buffer
	m_init(device, sizeof(struct deviceInfo));

	// read config
	rv = readconfig(configpath, config);
	if (rv) {
		ERR(rv);
		return -1;
	}
	// daemonize if supplied
	if (g_daemonize) {
		rv = become_daemon(*config);
		if (rv) {
			ERR(rv);
			return -1;
		}
	}

	// set 0 and -1 to non buffering
	setbuf(stdout, NULL);
	setbuf(stderr, NULL);

	// init signal handler
	rv = init_signalhandler(config);
	if (rv) {
		ERR(rv);
		return -1;
	}

	// init the udev monitor
	rv = init_udev(udev);
	if (rv) {
		ERR(rv);
		return -1;
	}

	// init keylogging if supplied
	rv = init_keylogging(NULL, kbd, config);
	if (rv) {
		ERR(rv);
		return -1;
	}

	// SETUP EPOLL
	*epollfd = epoll_create(1); // size gets ignored since kernel 2.6.8
	if (*epollfd < 0) {
		ERR(*epollfd);
		return -1;
	}

	memset_s(udevevent, sizeof(struct epoll_event), 0);
	udevevent->events = EPOLLIN;
	udevevent->data.fd = udev->udevfd;

	rv = epoll_ctl(*epollfd, EPOLL_CTL_ADD, udev->udevfd, udevevent);
	if (rv) {
		ERR(rv);
		return -1;
	}
	LOG(0, "Startup done!\n");
	return 0;
}

#ifndef RUN_UNIT_TESTS
int main(int argc, char *argv[])
{
	ssize_t rv;
	struct argInfo arg;
	struct udevInfo udev;
	struct configInfo config;
	struct keyboardInfo kbd;

	struct managedBuffer device;

	int epollfd = 0;
	struct epoll_event udevevent;

	// handle non root
	if (getuid() != 0) {
		LOG(-1, "Please restart this daemon as root!\n");
		return -1;
	}

	// set coredump limit
	{
		struct rlimit limit;
		limit.rlim_cur = 0;
		limit.rlim_max = 0;

		setrlimit(RLIMIT_CORE, &limit);
	}

	g_daemonize = false;
	g_loglevel = 0;

	// interpret args
	rv = handleargs(argc, argv, &arg);
	if (rv) {
		ERR(rv);
		return -1;
	}

	// initalize the daemon contexts
	rv = init(arg.configpath, &udev, &config, &kbd, &device, &epollfd,
		  &udevevent);
	if (rv) {
		ERR(rv);
		return -1;
	}

	// MAIN LOOP
	while (!g_brexit) {
		size_t i;
		int readfds;
		struct epoll_event events[MAX_SIZE_EVENTS];

		readfds = epoll_wait(epollfd, events, MAX_SIZE_EVENTS, -1);
		if (readfds < 0) {
			if (errno ==
			    EINTR) { // fix endless loop when receiving SIGHUP
				readfds = 0;

			} else {
				ERR(readfds);
				break;
			}
		}

		for (i = 0; i < (size_t)readfds; i++) {
			int fd = events[i].data.fd;

			if ((events[i].events & EPOLLIN) > 0) {
				if (fd ==
				    udev.udevfd) { // if a new event device has been added
					rv = handle_udevev(&device, &kbd,
							   &config, &udev,
							   epollfd);
					if (rv) {
						ERR(rv);
					}

				} else {
					struct input_event event;
					ssize_t size;

					size = read(fd, &event,
						    sizeof(struct input_event));
					if (size < 0) {
						ERR(size);

					} else {
						// handle keyboard grabbing
						if (event.type == EV_KEY) {
							LOG(2,
							    "fd=%d event.type=%d event.code=%d event.value=%d\n",
							    fd, event.type,
							    event.code,
							    event.value);

							if (event.value != 2) {
								if (m_deviceInfo(
									    &device)[fd]
										    .score >=
									    config.maxcount &&
								    event.value ==
									    0 &&
								    !m_deviceInfo(
									     &device)[fd]
									     .locked) {
									int ioctlarg =
										1;

									if (ioctl(fd,
										  EVIOCGRAB,
										  &ioctlarg)) {
										ERR("ioctl");
									}
									LOG(0,
									    "Locked fd %d\n",
									    fd);
									m_deviceInfo(
										&device)[fd]
										.locked =
										true;
								}

								if (logkey(&kbd,
									   &m_deviceInfo(
										   &device)
										   [fd],
									   event,
									   &config)) {
									LOG(0,
									    "logkey failed!\n");
								}
							}

						} else if (event.type ==
							   SYN_DROPPED) {
							LOG(-1,
							    "Sync dropped! Eventhandler not fast enough!\n");
						}
					}
				}
			}
			events[i].events = 0;
		}

		// reload config if SIGHUP is received
		if (g_reloadconfig) {
			LOG(0, "Reloading config file...\n\n");

			rv = init(arg.configpath, &udev, &config, &kbd, &device,
				  &epollfd, &udevevent);
			if (rv) {
				ERR(rv);
				return -1;
			}

			g_reloadconfig = false;
		}
	}

	// close all open file descriptors to event devnodes
	if (m_deviceInfo(&device) != NULL) {
		size_t i;

		for (i = 0; i < device.size; i++) {
			if (m_deviceInfo(&device)[i].fd != -1) {
				LOG(1, "fd %d still open!\n", i);
				rv = deinit_device(&m_deviceInfo(&device)[i],
						   &config, &kbd, epollfd);
				if (rv) {
					ERR(rv);
				}
			}
		}
		m_free(&device);
	}

	rv = close(epollfd);
	if (rv) {
		ERR(rv);
		return -1;
	}

	deinit_udev(&udev);

	deinit_keylogging(&kbd, &config);

	LOG(0, "All exit routines done!\n");
	return 0;
}
#else
int unit_test_main();

int main()
{
	return unit_test_main();
}
#endif
