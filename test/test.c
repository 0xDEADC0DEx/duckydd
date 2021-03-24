#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>

#include <cmocka.h>

void test_init_signalhandler(void **state);
void test_become_daemon_fail(void **state);
void test_become_daemon_success(void **state);

int unit_test_main()
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_init_signalhandler),
		cmocka_unit_test(test_become_daemon_fail),
		cmocka_unit_test(test_become_daemon_success),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
