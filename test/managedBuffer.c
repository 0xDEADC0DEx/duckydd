#include <stdint.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>

#include <cmocka.h>

#include "mbuffer.h"
#include "test_wrappers.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"

void test_mbuffer_success(void **state)
{
	struct managedBuffer buffer;
	m_init(&buffer, sizeof(char));

	assert_ptr_equal(buffer.b, NULL);
	assert_int_equal(buffer.size, 0);
	assert_int_equal(buffer.typesize, sizeof(char));

	assert_int_equal(m_realloc(&buffer, 20), 0);

	assert_ptr_not_equal(buffer.b, NULL);
	assert_int_equal(buffer.size, 20);
	assert_int_equal(buffer.typesize, sizeof(char));

	// segfaults here
	m_free(&buffer);

	assert_ptr_equal(buffer.b, NULL);
	assert_int_equal(buffer.size, 0);
	assert_int_equal(buffer.typesize, sizeof(char));
}

void test_mbuffer_fail(void **state)
{
	struct managedBuffer buffer;
	m_init(&buffer, sizeof(char));

	assert_ptr_equal(buffer.b, NULL);
	assert_int_equal(buffer.size, 0);
	assert_int_equal(buffer.typesize, sizeof(char));

	g_wrap = true;

	expect_value(__wrap_realloc, ptr, NULL);
	expect_value(__wrap_realloc, size, 20);
	will_return(__wrap_realloc, NULL);

	assert_int_equal(m_realloc(&buffer, 20), -1);

	g_wrap = false;

	assert_ptr_equal(buffer.b, NULL);
	assert_int_equal(buffer.size, 0);
	assert_int_equal(buffer.typesize, sizeof(char));

	m_free(&buffer);

	assert_ptr_equal(buffer.b, NULL);
	assert_int_equal(buffer.size, 0);
	assert_int_equal(buffer.typesize, sizeof(char));
}

#pragma GCC diagnostic pop
