
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "logger.h"
#include "vars.h"

void _logger(short lvl, const char format[], ...)
{
	// check for a format string bigger than the max
	if (lvl <= g_loglevel) {
		FILE *fd = NULL;
		va_list args;

		va_start(args, format);

		switch (lvl) {
		case -1:
			fd = stderr;
			break;

		default:
			fd = stdout;
			break;
		}

		vfprintf(fd, format, args);

		va_end(args);
	}
}
