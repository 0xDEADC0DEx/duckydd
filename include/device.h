#ifndef DEVICE_DUCKYDD_H
#define DEVICE_DUCKYDD_H

#include "mbuffer.h"
#include <stdbool.h>
#include <stdio.h>
#include <time.h>

#include <xkbcommon/xkbcommon.h>

#include "io.h"
#include "vars.h"
#include "mbuffer.h"

struct keyboardInfo;
struct udevInfo;

struct keystrokeDiff {
    struct timespec lasttime;
    struct managedBuffer strokesdiff; // holds time differences between keystrokes in queue
    size_t currdiff;
};

struct deviceInfo {
    char openfd[PATH_MAX];
    int fd;

    struct managedBuffer devlog; // holds keystrokes pressed

    union {
        struct xkb_state* xstate; // xkbcommon state

        //kstate in bits: Capslock / CtrlR / CtrlL / ShiftR / ShiftL / Alt / Control / AltGr / Shift
        //from keyboard.h
        uint16_t kstate;
    };

    int score;
    bool locked;

    struct keystrokeDiff timediff;
};

int deinit_device(struct deviceInfo *device, struct configInfo *config, struct keyboardInfo *kbd, const int epollfd);


int search_fd(struct managedBuffer *device, const char location[]);

int remove_fd(struct managedBuffer *device, struct configInfo *config, struct keyboardInfo *kbd, const int epollfd, const int fd);

int add_fd(struct managedBuffer *device, struct keyboardInfo *kbd, struct configInfo *config, const int epollfd, const char location[]);

int handle_udevev(struct managedBuffer *device, struct keyboardInfo *kbd, struct configInfo *config, struct udevInfo *udev, const int epollfd);

#endif
