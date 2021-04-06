
#include <libudev.h>

#include "io.h"
#include "udev.h"
#include "safe_lib.h"
#include "logger.h"

int has_tty(struct udevInfo udev)
{
	int rv;
	int err = 0;
	struct udev_enumerate *en;
	struct udev_list_entry *le;

	en = udev_enumerate_new(udev.udev);
	if (en == NULL) {
		ERR(en);
		err = -1;
		goto error_exit;
	}

	// match to the name starting with tty
	rv = udev_enumerate_add_match_subsystem(en, "tty");
	if (rv < 0) {
		ERR(rv);
		err = -2;
		goto error_exit;
	}

	// match to major and minor number of the device
	rv = udev_enumerate_add_match_property(
		en, "MAJOR", udev_device_get_property_value(udev.dev, "MAJOR"));
	if (rv < 0) {
		err = -3;
		goto error_exit;
	}

	rv = udev_enumerate_add_match_property(
		en, "MINOR", udev_device_get_property_value(udev.dev, "MINOR"));
	if (rv < 0) {
		ERR(rv);
		err = -4;
		goto error_exit;
	}

	// get iterator
	udev_enumerate_scan_devices(en);

	le = udev_enumerate_get_list_entry(
		en); // loop through all entities and search for a usb tty
	while (le != NULL) {
		char temp[10];

		// find ttyACM or ttyUSB devices
		strcpy_s(temp, 6, find_file(udev_list_entry_get_name(le)));
		if (strncmp(temp, "ttyACM", 6) == 0 ||
		    strncmp(temp, "ttyUSB", 6) == 0) {
			udev_enumerate_unref(en);
			return 1;
		}
		le = udev_list_entry_get_next(le);
	}

error_exit:
	udev_enumerate_unref(en);
	return err;
}

int init_udev(struct udevInfo *udev)
{
	int rv;

	udev->udev = udev_new();
	if (udev->udev == NULL) {
		ERR(udev->udev);
		return -1;
	}

	// create a new monitor from the context
	udev->mon = udev_monitor_new_from_netlink(udev->udev, "udev");
	if (udev->mon == NULL) {
		ERR(udev->mon);
		return -2;
	}

	// match only input devices
	rv = udev_monitor_filter_add_match_subsystem_devtype(udev->mon, "input",
							     NULL);
	if (rv < 0) {
		ERR(rv);
		return -3;
	}

	// start receiving events
	rv = udev_monitor_enable_receiving(udev->mon);
	if (rv < 0) {
		ERR(rv);
		return -4;
	}

	udev->udevfd = udev_monitor_get_fd(udev->mon);
	if (udev->udevfd == -1) {
		ERR(udev->udevfd);
		return -5;
	}
	return 0;
}

void deinit_udev(struct udevInfo *udev)
{
	udev_monitor_unref(udev->mon);
	udev_unref(udev->udev);
}
