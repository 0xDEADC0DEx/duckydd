#include <stdbool.h>

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>

#include <cmocka.h>

bool g_wrap = false;

void test_mbuffer_fail(void **state);
void test_mbuffer_success(void **state);

void test_init_signalhandler_fail(void **state);
void test_init_signalhandler_success(void **state);

void test_become_daemon_fail(void **state);
void test_become_daemon_success(void **state);

int unit_test_main()
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_mbuffer_fail),
		cmocka_unit_test(test_mbuffer_success),

		cmocka_unit_test(test_init_signalhandler_fail),
		cmocka_unit_test(test_init_signalhandler_success),

		cmocka_unit_test(test_become_daemon_fail),
		cmocka_unit_test(test_become_daemon_success),
	};

	print_message(
		"!!! Warnings from the daemon functions during these tests are expected and normal\n"
		"!!! Only pay attention if a test failed\n\n");

	return cmocka_run_group_tests(tests, NULL, NULL);
}
