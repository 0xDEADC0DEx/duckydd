
#include <stdio.h>
#include <stdlib.h>

#include <stdint.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>

#include <cmocka.h>

#include <unistd.h>

#include "daemon.h"
#include "io.h"
#include "test_wrappers.h"

static void init_struct(struct configInfo *config)
{
	config->maxcount = 0;
	strcpy_s(config->logpath, PATH_MAX, "\0");
	config->xkeymaps = false;

	config->minavrg.tv_nsec = 0;
	config->minavrg.tv_sec = 0;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"

void test_become_daemon_fail(void **state)
{
	struct configInfo config;

	init_struct(&config);

	// fail at first fork
	{
		will_return(__wrap_fork, -1);

		assert_int_equal(become_daemon(config), EXIT_FAILURE);
	}

	// fail at setsid
	{
		will_return(__wrap_fork, 0);
		will_return(__wrap_setsid, -1);

		assert_int_equal(become_daemon(config), EXIT_FAILURE);
	}

	// fail at second fork
	{
		will_return(__wrap_fork, 0);
		will_return(__wrap_setsid, 0);
		will_return(__wrap_fork, -1);

		assert_int_equal(become_daemon(config), EXIT_FAILURE);
	}

	// fail at chdir
	{
		will_return(__wrap_fork, 0);
		will_return(__wrap_setsid, 0);
		will_return(__wrap_fork, 0);

		expect_value(__wrap_chdir, path, '/');
		will_return(__wrap_chdir, -1);

		assert_int_equal(become_daemon(config), EXIT_FAILURE);
	}

	// fail at first fclose
	{
		will_return(__wrap_fork, 0);
		will_return(__wrap_setsid, 0);
		will_return(__wrap_fork, 0);

		expect_value(__wrap_chdir, path, '/');
		will_return(__wrap_chdir, 0);

		will_return(__wrap_fclose, -1);

		assert_int_equal(become_daemon(config), EXIT_FAILURE);
	}

	// fail at second fclose
	{
		will_return(__wrap_fork, 0);
		will_return(__wrap_setsid, 0);
		will_return(__wrap_fork, 0);

		expect_value(__wrap_chdir, path, '/');
		will_return(__wrap_chdir, 0);

		will_return(__wrap_fclose, 0);
		will_return(__wrap_fclose, -1);

		assert_int_equal(become_daemon(config), EXIT_FAILURE);
	}

	// fail at third fclose
	{
		will_return(__wrap_fork, 0);
		will_return(__wrap_setsid, 0);
		will_return(__wrap_fork, 0);

		expect_value(__wrap_chdir, path, '/');
		will_return(__wrap_chdir, 0);

		will_return(__wrap_fclose, 0);
		will_return(__wrap_fclose, 0);
		will_return(__wrap_fclose, -1);

		assert_int_equal(become_daemon(config), EXIT_FAILURE);
	}

	// fail at first freopen
	{
		will_return(__wrap_fork, 0);
		will_return(__wrap_setsid, 0);
		will_return(__wrap_fork, 0);

		expect_value(__wrap_chdir, path, '/');
		will_return(__wrap_chdir, 0);

		will_return(__wrap_fclose, 0);
		will_return(__wrap_fclose, 0);
		will_return(__wrap_fclose, 0);

		will_return(__wrap_freclose, -1);

		assert_int_equal(become_daemon(config), EXIT_FAILURE);
	}

	// fail at second freopen
	{
		will_return(__wrap_fork, 0);
		will_return(__wrap_setsid, 0);
		will_return(__wrap_fork, 0);

		expect_value(__wrap_chdir, path, '/');
		will_return(__wrap_chdir, 0);

		will_return(__wrap_fclose, 0);
		will_return(__wrap_fclose, 0);
		will_return(__wrap_fclose, 0);

		will_return(__wrap_freclose, 0);
		will_return(__wrap_freclose, -1);

		assert_int_equal(become_daemon(config), EXIT_FAILURE);
	}
}

void test_become_daemon_success(void **state)
{
	struct configInfo config;

	init_struct(&config);

	will_return(__wrap_fork, 0);
	will_return(__wrap_setsid, 0);
	will_return(__wrap_fork, 0);

	expect_value(__wrap_chdir, *path, '/');
	will_return(__wrap_chdir, 0);

	expect_value(__wrap_umask, mask, 0);
	will_return(__wrap_umask, 0);

	expect_value(__wrap_fclose, fp, stdout);
	will_return(__wrap_fclose, 0);

	expect_value(__wrap_fclose, fp, stdin);
	will_return(__wrap_fclose, 0);

	expect_value(__wrap_fclose, fp, stderr);
	will_return(__wrap_fclose, 0);

	expect_value(__wrap_freclose, path, "/out.log");
	expect_value(__wrap_freclose, mode, "w");
	expect_value(__wrap_freclose, stream, stdout);
	will_return(__wrap_freclose, stdout);

	expect_value(__wrap_freclose, path, "/out.log");
	expect_value(__wrap_freclose, mode, "w");
	expect_value(__wrap_freclose, stream, stderr);
	will_return(__wrap_freclose, stderr);

	assert_int_equal(become_daemon(config), EXIT_SUCCESS);
}
#pragma GCC diagnostic pop
