#ifndef UDEV_DUCKYDD_H
#define UDEV_DUCKYDD_H

struct udevInfo {
    int udevfd;
    struct udev* udev;
    struct udev_device* dev;
    struct udev_monitor* mon;
};


// checks if the device has a tty device associated with it
int has_tty(struct udevInfo udev);

// initalizer functions
int init_udev(struct udevInfo* udev);
void deinit_udev(struct udevInfo* udev);

#endif
