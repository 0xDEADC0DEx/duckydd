
#include <string.h>

#include <unistd.h>
#include <error.h>
#include <sys/epoll.h>
#include <error.h>
#include <libudev.h>
#include <fcntl.h>

#include <xkbcommon/xkbcommon-x11.h>

#include "udev.h"
#include "mbuffer.h"
#include "device.h"
#include "logkeys.h"
#include "config.h"
#include "logger.h"

#define PREFIX char
#define T char
#include "mbuffertemplate.h"

#define PREFIX deviceInfo
#define T struct deviceInfo
#include "mbuffertemplate.h"

#define PREFIX struct_timespec
#define T struct timespec
#include "mbuffertemplate.h"

int deinit_device(struct deviceInfo *device, struct configInfo *config,
		  struct keyboardInfo *kbd, const int epollfd)
{
	long int rv;
	int err = 0;

	if (device->fd != -1) {
		rv = epoll_ctl(epollfd, EPOLL_CTL_DEL, device->fd, NULL);
		if (rv) { // unregister the fd
			ERR(rv);
			err = -1;
			goto error_exit;
		}

		rv = close(device->fd);
		if (rv) { // close the fd
			ERR(rv);
			err = -2;
			goto error_exit;
		}

		device->openfd[0] = '\0';
		device->fd = -1;

		if (device->devlog.size != 0 &&
		    device->score >= config->maxcount) {
			char temp[100];
			time_t current_time = time(NULL);
			struct tm tm;

			localtime_r(&current_time, &tm);
			strftime(temp, sizeof(temp), " [%c]\n", &tm);

			LOG(2, "Writing devlog to logfile\n");

			rv = m_append_array_char(&device->devlog, temp,
						 strnlen_s(temp, 100));
			if (rv) {
				ERR(rv);
			}

			// write keylog to the log file
			rv = write(kbd->outfd, device->devlog.b,
				   device->devlog.size);
			if (rv < 0) {
				ERR(rv);
			}
		}
	}

	m_free(&device->devlog);

#ifdef ENABLE_XKB_EXTENSION
	if (config->xkeymaps && device->xstate != NULL) {
		xkb_state_unref(device->xstate);
		device->xstate = NULL;
	}
#endif

	m_free(&device->timediff.strokesdiff);
	return err;

error_exit:
	rv = close(device->fd);
	if (rv) {
		ERR(rv);
	}
	return err;
}

int search_fd(struct managedBuffer *device, const char location[])
{
	LOG(1, "Searching for: %s\n", location);

	if (location != NULL && device->size < INT_MAX) {
		int i;

		for (i = 0; i < (int)device->size;
		     i++) { // find the fd in the array
			if (strcmp_s(m_deviceInfo(device)[i].openfd, PATH_MAX,
				     location, 0) == 0 &&
			    m_deviceInfo(device)[i].fd != -1) {
				return i;
			}
		}
	}
	return -1;
}

int remove_fd(struct managedBuffer *device, struct configInfo *config,
	      struct keyboardInfo *kbd, const int epollfd, const int fd)
{
	ssize_t rv;

	LOG(1, "Trying to remove %d\n", fd);
	if (fd > -1) {
		rv = deinit_device(&m_deviceInfo(device)[fd], config, kbd,
				   epollfd);
		if (rv) {
			ERR(rv);
			return -1;
		}

		{
			size_t i;
			size_t bigsize = 0;

			// find the biggest fd in the array
			for (i = 0; i < device->size; i++) {
				if (m_deviceInfo(device)[i].openfd[0] != '\0') {
					bigsize = i + 1;
				}
			}

			// free the unnecessary space
			if (bigsize < device->size) {
				bool failed = false;

				for (i = bigsize; i < device->size; i++) {
					rv = deinit_device(
						&m_deviceInfo(device)[i],
						config, kbd, epollfd);
					if (rv) {
						ERR(rv);
						failed = true;
					}
				}

				if (!failed) {
					rv = m_realloc(device, bigsize);
					if (rv) {
						ERR(rv);
					}
				}
			}
		}

	} else {
		LOG(-1, "Did not find %d\n", fd);
		return -2;
	}

	LOG(1, "Removed %d\n\n", fd);

	return 0;
}

int add_fd(struct managedBuffer *device, struct keyboardInfo *kbd,
	   struct configInfo *config, const int epollfd, const char location[])
{
	ssize_t rv;
	int err = 0;
	int fd;

	LOG(1, "Adding: %s\n", location);

	// open a fd to the devnode
	fd = open(location, O_RDONLY | O_NONBLOCK);
	if (fd < 0) {
		ERR(fd);
		err = -1;
		goto error_exit;
	}

	// allocate more space if the fd doesn't fit
	if (device->size <= (size_t)fd && fd >= 0) {
		size_t i;
		size_t prevsize;

		prevsize = device->size;
		rv = m_realloc(device, (size_t)fd + 1);
		if (rv) {
			ERR(rv);
			err = -2;
			goto error_exit;
		}

		// initialize all members of the device array which haven't been used
		for (i = prevsize; i < device->size; i++) {
			memset_s(&m_deviceInfo(device)[i],
				 sizeof(struct deviceInfo), 0);
			m_deviceInfo(device)[i].fd = -1;

			m_init(&m_deviceInfo(device)[i].devlog, sizeof(char));
			m_init(&m_deviceInfo(device)[i].timediff.strokesdiff,
			       sizeof(struct timespec));
		}
	}

	// allocate and set the openfd
	if (m_deviceInfo(device)[fd].fd == -1) {
		size_t i;

		strcpy_s(m_deviceInfo(device)[fd].openfd, PATH_MAX, location);
		m_deviceInfo(device)[fd].fd = fd;

#ifdef ENABLE_XKB_EXTENSION
		if (config->xkeymaps) {
			// set the device state
			m_deviceInfo(device)[fd].xstate =
				xkb_x11_state_new_from_device(kbd->x.keymap,
							      kbd->x.con,
							      kbd->x.device_id);
			if (!m_deviceInfo(device)[fd].xstate) {
				ERR(m_deviceInfo(device)[fd].xstate);
				err = -3;
				goto error_exit;
			}
		}
#endif

		// allocate the array
		rv = m_realloc(&m_deviceInfo(device)[fd].timediff.strokesdiff,
			       6);
		if (rv) {
			ERR(rv);
			err = -4;
			goto error_exit;
		}

		// reset strokesdiff array
		for (i = 0;
		     i < m_deviceInfo(device)[fd].timediff.strokesdiff.size;
		     i++) {
			memset_s(&m_struct_timespec(
					 &m_deviceInfo(device)[fd]
						  .timediff.strokesdiff)[i],
				 sizeof(struct timespec), 0);
		}

		// add a new fd which should be polled
		{
			struct epoll_event event;
			memset_s(&event, sizeof(struct epoll_event), 0);
			event.events = EPOLLIN;
			event.data.fd = fd;

			rv = epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
			if (rv) {
				ERR(rv);
				err = -5;
				goto error_exit;
			}
		}
	} else {
		LOG(-1,
		    "Device fd already in use! There is something seriously wrong!\n");
		err = -5;
		goto error_exit;
	}
	LOG(1, "Added %i\n\n", fd);
	return fd;

error_exit:
	if (close(fd)) {
		LOG(-1, "close failed\n");
	}
	return err;
}

int handle_udevev(struct managedBuffer *device, struct keyboardInfo *kbd,
		  struct configInfo *config, struct udevInfo *udev,
		  const int epollfd)
{
	ssize_t rv;
	int err = 0;
	const char *devnode;
	const char *action;

	// get a device context from the monitor
	udev->dev = udev_monitor_receive_device(udev->mon);
	if (udev->dev == NULL) {
		ERR(udev->dev);
		err = -1;
		goto error_exit;
	}

	devnode = udev_device_get_devnode(udev->dev);
	action = udev_device_get_action(udev->dev);

	// device has a devnode
	if (devnode != NULL && action != NULL) {
		LOG(2, "%s %s -> %s [%s] | %s:%s\n",
		    udev_device_get_devpath(udev->dev), action, devnode,
		    udev_device_get_subsystem(udev->dev),
		    udev_device_get_property_value(udev->dev, "MAJOR"),
		    udev_device_get_property_value(udev->dev, "MINOR"));

		// add the devnode to the array
		if (strncmp(action, "add", 3) == 0) {
			int fd;

			fd = add_fd(device, kbd, config, epollfd, devnode);
			if (fd >= 0) {
				if (has_tty(*udev)) {
					m_deviceInfo(device)[fd].score++;
				}
			} else {
				ERR(fd);
				err = -2;
				goto error_exit;
			}

			// remove the fd from the device array
		} else if (strncmp(action, "remove", 6) == 0) {
			int fd;

			// search for the fd and remove it if possible
			fd = search_fd(device, devnode);
			if (fd >= 0) {
				rv = remove_fd(device, config, kbd, epollfd,
					       fd);
				if (rv) {
					ERR(rv);
					err = -3;
					goto error_exit;
				}
			}
		}
	}

error_exit:
	udev_device_unref(udev->dev);
	return err;
}
