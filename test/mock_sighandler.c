
#include <stdint.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>

#include <stdio.h>

#include <signal.h>

#include <cmocka.h>

#include "signalhandler.h"
#include "test_wrappers.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"

void test_init_signalhandler_success(void **state)
{
	size_t i;
	const uintmax_t signals[] = { SIGHUP, SIGTERM, SIGINT, SIGCHLD };

	for (i = 0; i < 4; i++) {
		expect_in_set(__wrap_sigaction, signum, signals);
	}

	//TODO expect_value doesnt register value as a parameter check (bug???)
	/*for (i = 0; i < 3; i++) {
		expect_value(__wrap_sigaction, act->sa_handler, handle_signal);
	}
	expect_value(__wrap_sigaction, act->sa_handler, SIG_IGN);*/

	for (i = 0; i < 4; i++) {
		expect_value(__wrap_sigaction, act->sa_flags, 0);
	}

	for (i = 0; i < 4; i++) {
		expect_value(__wrap_sigaction, oldact, NULL);
	}

	for (i = 0; i < 4; i++) {
		will_return(__wrap_sigaction, 0);
	}

	assert_int_equal(init_signalhandler(), 0);
}

void test_init_signalhandler_fail(void **state)
{
	size_t i, j;
	const uintmax_t signals[] = { SIGHUP, SIGTERM, SIGINT, SIGCHLD };

	for (i = 0; i < 4; i++) {
		print_message("Test Cycle: %zu\n", i);

		for (j = 0; j < i; j++) {
			expect_in_set(__wrap_sigaction, signum, signals);
			expect_value(__wrap_sigaction, act->sa_flags, 0);
			expect_value(__wrap_sigaction, oldact, NULL);

			will_return(__wrap_sigaction, 0);
		}

		expect_in_set(__wrap_sigaction, signum, signals);
		expect_value(__wrap_sigaction, act->sa_flags, 0);
		expect_value(__wrap_sigaction, oldact, NULL);

		will_return(__wrap_sigaction, -1);
		assert_int_equal(init_signalhandler(), -i - 1);
	}
}

#pragma GCC diagnostic pop
