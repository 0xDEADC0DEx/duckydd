#ifndef DUCKYDD_IO_H
#define DUCKYDD_IO_H

#include <inttypes.h>
#include <stdbool.h>
#include <time.h>
#include <limits.h>
#include <errno.h>

#include "safe_lib.h"
#include "vars.h"

// holds data read from the config file (mainly used by readconfig)
struct configInfo {
    long int maxcount;

    char logpath[PATH_MAX];

    // kbd
    bool xkeymaps;

    struct timespec minavrg;
};

// holds data that was parsed out by handleargs
struct argInfo {
    char configpath[PATH_MAX];
};

int readconfig(const char path[], struct configInfo* data);
int handleargs(int argc, char* argv[], struct argInfo* data);

// internal logger function
char* binexpand(uint8_t bin, size_t size);

errno_t pathcat(char path1[], const char path2[]);
errno_t pathcpy(char path1[], const char path2[]);

const char* find_file(const char* input);
#endif
