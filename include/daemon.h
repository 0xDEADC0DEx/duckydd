
#ifndef DUCKYDD_DAEMON_H
#define DUCKYDD_DAEMON_H

#include "vars.h"

struct configInfo;

int become_daemon(struct configInfo config);

#endif
