
#include <stdint.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>

#include <cmocka.h>

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>

#include "test_wrappers.h"

int __wrap_sigaction(int signum, const struct sigaction *act,
		     struct sigaction *oldact)
{
	check_expected(signum);
	//check_expected(act->sa_handler);
	check_expected(act->sa_flags);
	check_expected_ptr(oldact);

	return mock();
}

pid_t __wrap_fork(void)
{
	return mock();
}

pid_t __wrap_setsid(void)
{
	return mock();
}

int __wrap_chdir(const char *path)
{
	check_expected_ptr(path);

	return mock();
}

mode_t __wrap_umask(mode_t mask)
{
	check_expected(mask);
	return mock();
}

int __wrap_fclose(FILE *fp)
{
	check_expected_ptr(fp);
	return mock();
}

FILE *__wrap_freopen(const char *path, const char *mode, FILE *stream)
{
	check_expected_ptr(path);
	check_expected_ptr(mode);
	check_expected_ptr(stream);
	return (FILE *)mock();
}
