#ifndef TEST_WRAPPERS_H
#define TEST_WRAPPERS_H

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>

struct sigaction;

int __wrap_sigaction(int signum, const struct sigaction *act, struct sigaction *oldact);

pid_t __wrap_fork(void);
pid_t __wrap_setsid(void);

mode_t __wrap_umask(mode_t mask);
int __wrap_chdir(const char *path);
int __wrap_fclose(FILE *fp);
FILE *__wrap_freopen(const char *path, const char *mode, FILE *stream);
#endif
