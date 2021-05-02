
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <linux/kd.h>
#include <linux/keyboard.h>
#include <unistd.h>
#include <xcb/xcb.h>
#include <xkbcommon/xkbcommon-x11.h>
#include <xkbcommon/xkbcommon.h>

#include "config.h"
#include "io.h"
#include "logkeys.h"
#include "mbuffer.h"
#include "device.h"
#include "logger.h"

#define PREFIX char
#define T char
#include "mbuffertemplate.h"

#define PREFIX struct_timespec
#define T struct timespec
#include "mbuffertemplate.h"

enum {
	KEY_STATE_RELEASE = 0,
	KEY_STATE_PRESS = 1,
	KEY_STATE_REPEAT = 2,
};

static const size_t conpath_size = 6;
static const char *conpath[] = {
	"/proc/self/fd/0", "/dev/tty",	   "/dev/tty0", "/dev/vc/0",
	"/dev/systty",	   "/dev/console", NULL
};

static int open_console0()
{
	size_t i;

	// go through some console paths
	for (i = 0; i < conpath_size; i++) {
		int fd;

		fd = open(conpath[i], O_NOCTTY | O_RDONLY | O_NOFOLLOW);
		if (fd >= 0) {
			char ioctlarg;

			// if it is a tty and has the right keyboard return the fd
			if (!ioctl(fd, KDGKBTYPE, &ioctlarg)) {
				if (isatty(fd) && ioctlarg == KB_101) {
					return fd;
				}
			}

			if (close(fd)) {
				ERR("close");
			}
		}

		LOG(1, "Failed to open %s! Trying next one ...\n", conpath[i]);
	}

	return -1;
}

#ifdef ENABLE_XKB_EXTENSION
static int load_x_keymaps(const char screen[], struct keyboardInfo *kbd,
			  struct configInfo *config)
{
	ssize_t rv;
	int err = 0;

	// initalize x keymap
	kbd->x.keymap = NULL;

	// create a new context
	kbd->x.ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
	if (!kbd->x.ctx) {
		ERR(kbd->x.ctx);
		err = -1;
		goto error_exit;
	}

	// connect to the x server
	kbd->x.con = xcb_connect(screen, NULL);
	if (kbd->x.con == NULL) {
		ERR(kbd->x.con);
		err = -2;
		goto error_exit;
	}

	rv = xcb_connection_has_error(kbd->x.con);
	if (rv) {
		LOG(-1, "xcb_connection_has_error failed!\n");
		err = -3;
		goto error_exit;
	}

	// setup the xkb extension
	{
		uint16_t major_xkb, minor_xkb;
		uint8_t event_out, error_out;
		rv = xkb_x11_setup_xkb_extension(kbd->x.con,
						 XKB_X11_MIN_MAJOR_XKB_VERSION,
						 XKB_X11_MIN_MINOR_XKB_VERSION,
						 0, &major_xkb, &minor_xkb,
						 &event_out, &error_out);
		if (!rv) {
			ERR(rv);
			err = -4;
			goto error_exit;
		}
	}

	// get device id of the core x keyboard
	kbd->x.device_id = xkb_x11_get_core_keyboard_device_id(kbd->x.con);
	if (kbd->x.device_id == -1) {
		ERR(kbd->x.device_id);
		err = -5;
		goto error_exit;
	}

	// get keymap
	kbd->x.keymap =
		xkb_x11_keymap_new_from_device(kbd->x.ctx, kbd->x.con,
					       kbd->x.device_id,
					       XKB_KEYMAP_COMPILE_NO_FLAGS);
	if (!kbd->x.keymap) {
		ERR(kbd->x.keymap);
		err = -6;
		goto error_exit;
	}

	return err;

error_exit:
	config->xkeymaps = false;

	// get rid of unused heap space
	if (kbd->x.keymap) {
		xkb_keymap_unref(kbd->x.keymap);
	}

	if (kbd->x.ctx) {
		xkb_context_unref(kbd->x.ctx);
	}
	return err;
}
#endif

static int load_kernel_keymaps(const int fd, struct keyboardInfo *kbd)
{
	ssize_t rv;
	unsigned int i, k;

	// load scancode to keycode table
	for (i = 0; i < MAX_SIZE_SCANCODE; i++) {
		struct kbkeycode temp;

		temp.scancode = i;
		temp.keycode = 0;

		rv = ioctl(fd, KDGETKEYCODE, &temp);
		if (rv) {
			ERR(rv);
			return -1;
		}

		kbd->k.keycode[i] = temp.keycode;
	}

	// load keycode to actioncode table
	for (i = 0; i < MAX_NR_KEYMAPS; i++) {
		for (k = 0; k < NR_KEYS; k++) {
			if (i < CHAR_MAX && k < CHAR_MAX) {
				struct kbentry temp;

				temp.kb_table = (unsigned char)i;
				temp.kb_index = (unsigned char)k;
				temp.kb_value = 0;

				rv = ioctl(fd, KDGKBENT, &temp);
				if (rv) {
					ERR("ioctl");
					return -2;
				}

				kbd->k.actioncode[i][k] = temp.kb_value;
			}
		}
	}

	// loads actioncode to string table
	for (i = 0; i < MAX_NR_FUNC; i++) {
		if (i < CHAR_MAX) {
			struct kbsentry temp;

			temp.kb_func = (unsigned char)i;
			temp.kb_string[0] = '\0';

			rv = ioctl(fd, KDGKBSENT, &temp);
			if (rv) {
				ERR("ioctl");
				return -3;
			}

			memcpy_s(kbd->k.string[i], MAX_SIZE_KBSTRING,
				 temp.kb_string, MAX_SIZE_KBSTRING);
		}
	}

	return 0;
}

// translate scancode to string
static int interpret_keycode(struct managedBuffer *buff,
			     struct deviceInfo *device,
			     struct keyboardInfo *kbd, unsigned int code,
			     uint8_t value)
{
	ssize_t rv;
	uint16_t modmask = 0;

	LOG(2, "keycode:%d value:%d - typ:%d val:%d\n", code, value, KTYP(code),
	    KVAL(code));

	// Capslock / CtrlR / CtrlL / ShiftR / ShiftL / Alt / Control / AltGr / Shift
	switch (code) { //  (1 <= scancode <= 88) -> keycode == scancode
	case KEY_RIGHTSHIFT:
	case KEY_LEFTSHIFT:
		if (code == KEY_RIGHTSHIFT) {
			modmask |= 1 << 5;
		} else {
			modmask |= 1 << 4;
		}
		modmask |= 1;
		break;

	case KEY_RIGHTCTRL:
	case KEY_LEFTCTRL:
		if (code == KEY_RIGHTCTRL) {
			modmask |= 1 << 7;
		} else {
			modmask |= 1 << 6;
		}
		modmask |= 1 << 2;
		break;

	case KEY_RIGHTALT:
	case KEY_LEFTALT:
		modmask |= 1 << 3;
		break;

		/*case ??: //TODO
                modmask |= 1 << 1;*/

	case KEY_CAPSLOCK:
		modmask |= 1 << 8;
		break;
	}

	if (value) {
		// change modifier state
		device->kstate |= modmask;

	} else {
		device->kstate &= (0xffff ^ modmask);
	}

	if (!modmask) {
		// not a mod key
		if (value == KEY_STATE_PRESS) {
			// is not multi-character symbol
			if (code < 0x1000) {
				unsigned short actioncode;

				if (KTYP(code) != KT_META) {
					actioncode =
						kbd->k.actioncode
							[device->kstate]
							[kbd->k.keycode[code]];
				} else {
					return 0;
				}

				LOG(1, "actioncode:%d -> %c\n", actioncode,
				    KVAL(actioncode));

				rv = m_append_member_char(buff,
							  KVAL(actioncode));
				if (rv) {
					ERR(rv);
				}

			} else { // TODO UNTESTED
				unsigned short actioncode;
				size_t i;
				unsigned short limit;

				code ^= 0xf000;
				actioncode =
					kbd->k.actioncode[device->kstate]
							 [kbd->k.keycode[code]];
				limit = 0x0;

				for (i = 0; i < 4;
				     i++) { // write unicode character utf-8 encoded into the logbuffer
					limit |= 0xff << i;
					if (code > limit) {
						rv = m_append_member_char(
							buff,
							actioncode &
								(0xff
								 << (7 * i)));
						if (rv) {
							ERR(rv);
						}
					}
				}
			}
		}

	} else {
		LOG(1, "kstate:%x\n", modmask);
	}
	return 0;
}

int init_keylogging(const char input[], struct keyboardInfo *kbd,
		    struct configInfo *config)
{
	ssize_t rv;
	int err = 0;

	// setup x keymaps
	if (config->xkeymaps) {
#ifdef ENABLE_XKB_EXTENSION
		rv = load_x_keymaps(input, kbd, config);
		if (rv) {
			ERR(rv);
		}
#else
		LOG(0, "The daemon was compiled without xkb support!\n");
#endif
	}

	// use kernel keymaps
	if (!config->xkeymaps) {
		int fd;

		// get a file descriptor to console 0
		fd = open_console0();
		if (fd < 0) {
			ERR(fd);
			LOG(-1,
			    "Failed to open a file descriptor to a vaild console!\n");
			err = -1;
			goto error_exit;
		}

		// load keytable / accent table / and scancode translation table
		rv = load_kernel_keymaps(fd, kbd);
		if (rv) {
			ERR(rv);
			err = -2;
			goto error_exit;
		}
	}

	// create keylog file
	{
		char path[PATH_MAX] = { '\0' };

		strcpy_s(path, PATH_MAX, config->logpath);
		strcat_s(path, PATH_MAX, "/key.log");

		kbd->outfd =
			open(path, O_WRONLY | O_APPEND | O_CREAT | O_NOCTTY,
			     S_IRUSR | S_IWUSR | O_NOFOLLOW);
		if (kbd->outfd < 0) {
			ERR("open");
			err = -3;
			goto error_exit;
		}
	}

	LOG(1, "Keylogging ready!\n");
	return err;

error_exit:
	LOG(-1,
	    "Exiting because the init of both the kernel keymaps and libxkbcommon failed!\n");
	return err;
}

int deinit_keylogging(struct keyboardInfo *kbd, struct configInfo *config)
{
	if (close(kbd->outfd)) {
		ERR("close");
	}

#ifdef ENABLE_XKB_EXTENSION
	if (config->xkeymaps) {
		xkb_keymap_unref(kbd->x.keymap);
		xkb_context_unref(kbd->x.ctx);
		xcb_disconnect(kbd->x.con);
	}
#endif
	return 0;
}

static inline void timespec_diff(struct timespec *result, struct timespec *a,
				 struct timespec *b)
{
	result->tv_sec = a->tv_sec - b->tv_sec;
	result->tv_nsec = a->tv_nsec - b->tv_nsec;

	if (result->tv_nsec < 0) {
		result->tv_sec--;
		result->tv_nsec += 1000000000L;
	}
}

static int check_if_evil(struct deviceInfo *device, struct configInfo *config)
{
	ssize_t rv;

	// increment currdiff until wraparound
	if (device->timediff.currdiff < device->timediff.strokesdiff.size) {
		struct timespec temp;

		// get current time
		rv = clock_gettime(CLOCK_REALTIME, &temp);
		if (rv) {
			ERR("clock_gettime");
			return -1;
		}

		// caluclate time difference
		timespec_diff(&m_struct_timespec(&device->timediff.strokesdiff)
				      [device->timediff.currdiff],
			      &temp, &device->timediff.lasttime);

		// save last value
		device->timediff.lasttime.tv_sec = temp.tv_sec;
		device->timediff.lasttime.tv_nsec = temp.tv_nsec;

		LOG(2, "lasttime: %ds %dns currdiff: %d\n", temp.tv_sec,
		    temp.tv_nsec, device->timediff.currdiff);

		// if the queue is filled then use it to calculate the average difference
		if (device->timediff.currdiff ==
			    device->timediff.strokesdiff.size - 1 &&
		    device->timediff.strokesdiff.size > 0) {
			size_t i;
			struct timespec sum;

			sum.tv_sec = 0;
			sum.tv_nsec = 0;

			// calculate average of the array
			for (i = 0; i < device->timediff.strokesdiff.size;
			     i++) {
				struct timespec curr;

				curr = m_struct_timespec(
					&device->timediff.strokesdiff)[i];

				sum.tv_sec += curr.tv_sec;
				sum.tv_nsec += curr.tv_nsec;
			}

			sum.tv_sec /= device->timediff.strokesdiff.size;
			sum.tv_nsec /= device->timediff.strokesdiff.size;

			LOG(2, "avgtime: %ds %dns\n", sum.tv_sec, sum.tv_nsec);

			if (sum.tv_sec < config->minavrg.tv_sec ||
			    (sum.tv_sec == config->minavrg.tv_sec &&
			     sum.tv_nsec < config->minavrg.tv_nsec)) {
				device->score++;
			}
		}
		device->timediff.currdiff++;

	} else {
		device->timediff.currdiff = 0;
	}

	return 0;
}

int logkey(struct keyboardInfo *kbd, struct deviceInfo *device,
	   struct input_event event, struct configInfo *config)
{
	ssize_t rv;
#ifdef ENABLE_XKB_EXTENSION
	// if xkbcommon lib has initalized
	if (config->xkeymaps) {
		xkb_keycode_t keycode = event.code + 8;

		// if key repeates
		if (event.value == KEY_STATE_REPEAT &&
		    !xkb_keymap_key_repeats(kbd->x.keymap, event.code)) {
			return 0;
		}

		// track key releases
		if (event.value == KEY_STATE_RELEASE) {
			xkb_state_update_key(device->xstate, keycode,
					     XKB_KEY_UP);
		} else {
			size_t size = 0;

			xkb_state_update_key(device->xstate, keycode,
					     XKB_KEY_DOWN);

			// get the size of the required buffer
			rv = xkb_state_key_get_utf8(device->xstate, keycode,
						    NULL, 0) +
			     1;

			if (rv >= 0) {
				size = (size_t)rv;
			} else {
				ERR("xkb_state_key_get_utf8");
				return -1;
			}

			if (size > 1) {
				char *buffer;

				buffer = malloc(size);
				if (buffer == NULL) {
					ERR("malloc");
					return -2;
				}

				// get the buffers
				xkb_state_key_get_utf8(device->xstate, keycode,
						       buffer, size);

				rv = m_append_array_char(&device->devlog,
							 buffer, size - 1);
				if (rv) { // dont copy \0
					ERR(rv);
					free(buffer);
					return -3;
				}
				free(buffer);

			} else if (size != 1) {
				LOG(-1,
				    "Keyevent without valid key in map (%d)!\n",
				    keycode);
			}
		}
	}
#endif

	if (!config->xkeymaps) {
		// interpret the keycode using the kernel keytable
		rv = interpret_keycode(&device->devlog, device, kbd, event.code,
				       event.value);
		if (rv) {
			ERR(rv);
			return -4;
		}
	}

	// check if the keystrokes are typed in a manner not typical to human typing
	rv = check_if_evil(device, config);
	if (rv) {
		ERR(rv);
		return -4;
	}

	// rotate keylog after 100 chars in the buffer
	if (device->devlog.size >= 20) {
		if (device->score >= config->maxcount) {
			LOG(2, "Writing devlog to logfile\n");

			// write keylog to the log file
			rv = write(kbd->outfd, device->devlog.b,
				   device->devlog.size);
			if (rv < 0) {
				ERR(rv);
			}
		}
		m_realloc(&device->devlog, 0);
	}
	return 0;
}
