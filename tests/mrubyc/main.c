/* SPDX-License-Identifier: BSD-3-Clause */

#include <stdint.h>
#include <mrubyc.h>
#include <zephyr/kernel.h>
#include <zephyr/ztest.h>

#include "extended_mrb.h"
#include "timer_mrb.h"

BUILD_ASSERT(MAX_VM_COUNT == CONFIG_MRUBYC_MAX_VM_COUNT);
BUILD_ASSERT(MAX_REGS_SIZE == CONFIG_MRUBYC_MAX_REGS_SIZE);
BUILD_ASSERT(MAX_SYMBOLS_COUNT == CONFIG_MRUBYC_MAX_SYMBOLS_COUNT);
BUILD_ASSERT(MRBC_USE_FLOAT == (IS_ENABLED(CONFIG_MRUBYC_FLOAT_SINGLE) ? 1 : 2));
BUILD_ASSERT(MRBC_USE_MATH == IS_ENABLED(CONFIG_MRUBYC_USE_MATH));
BUILD_ASSERT(MRBC_USE_STRING == 1);
BUILD_ASSERT(MRBC_USE_STRING_UTF8 == IS_ENABLED(CONFIG_MRUBYC_USE_STRING_UTF8));
BUILD_ASSERT(MRBC_USE_UNICODE_CASE == IS_ENABLED(CONFIG_MRUBYC_USE_UNICODE_CASE));
#if defined(MRBC_INT64) != defined(CONFIG_MRUBYC_INT64)
#error "The application and mruby/c disagree on integer width"
#endif
#if defined(MRBC_SUPPORT_OP_EXT) != defined(CONFIG_MRUBYC_SUPPORT_OP_EXT)
#error "The application and mruby/c disagree on extended operands"
#endif

#if defined(EXPECT_DEFAULTS)
BUILD_ASSERT(MAX_VM_COUNT == 5 && MAX_REGS_SIZE == 110 && MAX_SYMBOLS_COUNT == 255);
BUILD_ASSERT(MRBC_USE_FLOAT == 2 && MRBC_USE_MATH == 0);
BUILD_ASSERT(MRBC_USE_STRING_UTF8 == 0 && MRBC_USE_UNICODE_CASE == 0);
BUILD_ASSERT(MRBC_SUPPORT_OP_EXT == 1);
#if defined(MRBC_INT64)
#error "64-bit Ruby integers must be opt-in"
#endif
#endif

ZTEST(mrubyc, test_loader_timer_and_extended_operands)
{
	static _Alignas(8) uint8_t pool[32 * 1024];
	static const uint8_t rite0400_header[20] = "RITE0400";

	mrbc_init(pool, sizeof(pool));
	zassert_is_null(mrbc_create_task(rite0400_header, NULL),
			"The RITE0300 runtime must reject a RITE0400 header");

	zassert_not_null(mrbc_create_task(timer_mrb, NULL));
	int64_t start = k_uptime_get();

	zassert_equal(mrbc_run(), 0);
	int64_t elapsed = k_uptime_get() - start;
	const mrbc_value *timer = mrbc_get_global(mrbc_search_symid("$timer_result"));

	zassert_not_null(timer);
	zassert_equal(mrbc_type(*timer), MRBC_TT_INTEGER);
	zassert_equal(mrbc_integer(*timer), 42);
	zassert_true(elapsed >= 20 && elapsed < 200, "sleep_ms took %lld ms", (long long)elapsed);

	zassert_not_null(mrbc_create_task(extended_mrb, NULL));
	int result = mrbc_run();
	mrbc_sym symbol = mrbc_search_symid("$extended_result");

	if (IS_ENABLED(CONFIG_MRUBYC_SUPPORT_OP_EXT)) {
		zassert_equal(result, 0);
		zassert_true(symbol >= 0);
		const mrbc_value *extended = mrbc_get_global(symbol);

		zassert_not_null(extended);
		zassert_equal(mrbc_type(*extended), MRBC_TT_STRING);
		zassert_str_equal(mrbc_string_cstr(extended), "extension worked");
	} else {
		zassert_not_equal(result, 0);
		zassert_true(symbol < 0 || mrbc_get_global(symbol) == NULL);
	}
}

ZTEST_SUITE(mrubyc, NULL, NULL, NULL, NULL, NULL);
