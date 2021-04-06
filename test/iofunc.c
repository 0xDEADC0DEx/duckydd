#include <stdint.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>

#include <cmocka.h>

#include "io.h"
#include "test_wrappers.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"

void test_find_file_success(void **state)
{
	char test[] = { "/home/dingdong/file.txt" };

	assert_ptr_equal(find_file(test), &test[15]);
}

void test_find_file_fail(void **state)
{
	char test[] = { "home\0" };

	assert_ptr_equal(find_file(test), NULL);
}

#pragma GCC diagnostic pop
