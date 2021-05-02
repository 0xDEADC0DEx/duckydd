#ifndef LOGGER_H
#define LOGGER_H

#include <string.h>
#include <errno.h>

#define LOG(loglvl, format, ...) _logger(loglvl, format, ##__VA_ARGS__) // print function name
#define ERR(rv)_logger(-1, "[!] %s:%d \t(last no: %d -> %s | returned: %d)\n", __func__, __LINE__, errno, strerror(errno), rv);

void _logger(short lvl, const char format[], ...);

#endif
