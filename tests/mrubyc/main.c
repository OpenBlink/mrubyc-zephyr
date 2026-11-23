/* SPDX-License-Identifier: BSD-3-Clause */

#include <stdint.h>
#include <string.h>
#include <mrubyc.h>
#include <zephyr/kernel.h>
#include <zephyr/ztest.h>

#include "extended_mrb.h"
#include "rite0300_mrb.h"
#include "timer_mrb.h"

BUILD_ASSERT(MAX_VM_COUNT == CONFIG_MRUBYC_MAX_VM_COUNT);
BUILD_ASSERT(MAX_REGS_SIZE == CONFIG_MRUBYC_MAX_REGS_SIZE);
BUILD_ASSERT(MAX_SYMBOLS_COUNT == CONFIG_MRUBYC_MAX_SYMBOLS_COUNT);
BUILD_ASSERT(MRBC_USE_FLOAT == (IS_ENABLED(CONFIG_MRUBYC_FLOAT_SINGLE) ? 1 : 2));
BUILD_ASSERT(MRBC_USE_MATH == IS_ENABLED(CONFIG_MRUBYC_USE_MATH));
BUILD_ASSERT(MRBC_USE_STRING == 1);
BUILD_ASSERT(MRBC_USE_STRING_UTF8 == IS_ENABLED(CONFIG_MRUBYC_USE_STRING_UTF8));
BUILD_ASSERT(MRBC_USE_UNICODE_CASE == IS_ENABLED(CONFIG_MRUBYC_USE_UNICODE_CASE));
BUILD_ASSERT(MRBC_INSTANCE_DESTRUCTOR == IS_ENABLED(CONFIG_MRUBYC_INSTANCE_DESTRUCTOR));
BUILD_ASSERT(MRBC_EXCEPTION_CALL_NEST_LEVEL == CONFIG_MRUBYC_EXCEPTION_CALL_NEST_LEVEL);
BUILD_ASSERT(MRBC_TASK_NAME_LEN == CONFIG_MRUBYC_TASK_NAME_LEN);
BUILD_ASSERT(MRBC_KV_SIZE_INIT == CONFIG_MRUBYC_KV_SIZE_INIT);
BUILD_ASSERT(MRBC_KV_SIZE_INCREMENT == CONFIG_MRUBYC_KV_SIZE_INCREMENT);
BUILD_ASSERT(MRBC_TICK_UNIT == CONFIG_MRUBYC_TICK_UNIT_MS);
BUILD_ASSERT(MRBC_TIMESLICE_TICK_COUNT == CONFIG_MRUBYC_TIMESLICE_TICK_COUNT);
BUILD_ASSERT(MRBC_SCHEDULER_EXIT == IS_ENABLED(CONFIG_MRUBYC_SCHEDULER_EXIT));
#if defined(MRBC_SYMBOL_SEARCH_LINEAR) != defined(CONFIG_MRUBYC_SYMBOL_SEARCH_LINEAR)
#error "The application and mruby/c disagree on symbol search"
#endif
#if defined(MRBC_INT64) != defined(CONFIG_MRUBYC_INT64)
#error "The application and mruby/c disagree on integer width"
#endif
#if defined(MRBC_SUPPORT_OP_EXT) != defined(CONFIG_MRUBYC_SUPPORT_OP_EXT)
#error "The application and mruby/c disagree on extended operands"
#endif
#if defined(MRBC_USE_ALLOC_PROF) != defined(CONFIG_MRUBYC_ALLOC_PROFILING)
#error "The application and mruby/c disagree on allocator profiling"
#endif
#if defined(MRBC_NO_STDIO) != defined(CONFIG_MRUBYC_NO_STDIO) || \
	defined(MRBC_CONVERT_CRLF) != defined(CONFIG_MRUBYC_CONVERT_CRLF) || \
	defined(MRBC_DEBUG_REGS) != defined(CONFIG_MRUBYC_DEBUG_REGS)
#error "The application and mruby/c disagree on HAL or diagnostics configuration"
#endif
#if defined(GET_INT_ARG) == defined(CONFIG_MRUBYC_DISABLE_LEGACY_VALUE_MACROS)
#error "The legacy value macros must follow Kconfig"
#endif

#if defined(EXPECT_DEFAULTS)
BUILD_ASSERT(MAX_VM_COUNT == 5 && MAX_REGS_SIZE == 110 && MAX_SYMBOLS_COUNT == 255);
BUILD_ASSERT(MRBC_USE_FLOAT == 2 && MRBC_USE_MATH == 0);
BUILD_ASSERT(MRBC_USE_STRING_UTF8 == 0 && MRBC_USE_UNICODE_CASE == 0);
BUILD_ASSERT(MRBC_INSTANCE_DESTRUCTOR == 1);
BUILD_ASSERT(MRBC_EXCEPTION_CALL_NEST_LEVEL == 8 && MRBC_TASK_NAME_LEN == 15);
BUILD_ASSERT(MRBC_TICK_UNIT == 1 && MRBC_TIMESLICE_TICK_COUNT == 10);
BUILD_ASSERT(MRBC_SCHEDULER_EXIT == 1 && !IS_ENABLED(CONFIG_MRUBYC_SYMBOL_SEARCH_LINEAR));
BUILD_ASSERT(MRBC_SUPPORT_OP_EXT == 1);
BUILD_ASSERT(MRBC_KV_SIZE_INIT == 2 && MRBC_KV_SIZE_INCREMENT == 5);
BUILD_ASSERT(sizeof(MRBC_STRING_SIZE_T) == sizeof(uint16_t));
#if defined(MRBC_INT64) || defined(MRBC_USE_ALLOC_PROF) || \
	defined(MRBC_NO_STDIO) || defined(MRBC_CONVERT_CRLF) || \
	defined(MRBC_DEBUG_REGS) || !defined(GET_INT_ARG)
#error "Unexpected upstream defaults"
#endif
#endif

static _Alignas(8) uint8_t pool[32 * 1024];

static void check_timer_result(void)
{
	const mrbc_value *timer = mrbc_get_global(mrbc_search_symid("$timer_result"));

	zassert_not_null(timer);
	zassert_equal(mrbc_type(*timer), MRBC_TT_INTEGER);
	zassert_equal(mrbc_integer(*timer), 42, "Array pattern matching must bind both values");
}

#if defined(CONFIG_MRUBYC_SCHEDULER_EXIT)
#if MRBC_INSTANCE_DESTRUCTOR
static unsigned int destructor_calls;

static void count_destructor(mrbc_value *instance)
{
	ARG_UNUSED(instance);
	++destructor_calls;
}
#endif

static void check_instance_release(void)
{
	mrbc_class *cls = mrbc_define_class(NULL, "ReleaseProbe", NULL);

	zassert_not_null(cls);
#if MRBC_INSTANCE_DESTRUCTOR
	mrbc_define_destructor(cls, count_destructor);
#endif
	mrbc_value instance = mrbc_instance_new(NULL, cls, 0);

	zassert_equal(mrbc_type(instance), MRBC_TT_OBJECT);

	/* Insert distinct dynamic symbols and check lookups after the table grows. */
	for (int i = 0; i <= MRBC_KV_SIZE_INIT; i++) {
		char name[24];

		snprintk(name, sizeof(name), "probe_ivar_%d", i);
		mrbc_value symbol = mrbc_symbol_new(NULL, name);
		mrbc_value value = mrbc_integer_value(i);

		zassert_equal(mrbc_type(symbol), MRBC_TT_SYMBOL);
		mrbc_instance_setiv(&instance, mrbc_symbol(symbol), &value);
	}
	zassert_equal(instance.instance->ivar.data_size,
		      MRBC_KV_SIZE_INIT + MRBC_KV_SIZE_INCREMENT);

	for (int i = MRBC_KV_SIZE_INIT; i >= 0; i--) {
		char name[24];

		snprintk(name, sizeof(name), "probe_ivar_%d", i);
		mrbc_sym id = mrbc_search_symid(name);

		zassert_true(id >= 0);
		zassert_str_equal(mrbc_symid_to_str(id), name);
		zassert_equal(mrbc_str_to_symid(name), id);
		mrbc_value value = mrbc_instance_getiv(&instance, id);

		zassert_equal(mrbc_type(value), MRBC_TT_INTEGER);
		zassert_equal(mrbc_integer(value), i);
	}

	mrbc_decref(&instance);
#if MRBC_INSTANCE_DESTRUCTOR
	zassert_equal(destructor_calls, 1);
#endif
}

static void check_optional_features(void)
{
	static const char *const console_methods[] = {"p", "print", "puts", "printf"};
	mrbc_method method;

	for (size_t i = 0; i < ARRAY_SIZE(console_methods); i++) {
		mrbc_sym id = mrbc_search_symid(console_methods[i]);

		zassert_equal(id >= 0 && mrbc_find_method(&method, MRBC_CLASS(Object), id) != NULL,
			      !IS_ENABLED(CONFIG_MRUBYC_NO_STDIO), "%s", console_methods[i]);
	}

#if defined(CONFIG_MRUBYC_ALLOC_PROFILING)
	struct MRBC_ALLOC_PROF profile;

	mrbc_alloc_start_profiling();
	void *block = mrbc_raw_alloc(128);

	zassert_not_null(block);
	mrbc_raw_free(block);
	mrbc_alloc_stop_profiling();
	mrbc_alloc_get_profiling(&profile);
	zassert_true(profile.max >= profile.initial + 128);
	zassert_equal(profile.min, profile.initial);
#endif
}

ZTEST(mrubyc, test_loader_timer_and_extended_operands)
{
	mrbc_init(pool, sizeof(pool));
	zassert_is_null(mrbc_create_task(rite0300_mrb, NULL),
			"The RITE0400 runtime must reject bytecode from v3.4.1");
	check_instance_release();
	check_optional_features();

	mrbc_tcb *task = mrbc_create_task(timer_mrb, NULL);
	static const char task_name[] = "integration-task-with-a-long-name";
	const size_t name_length = MIN(sizeof(task_name) - 1, MRBC_TASK_NAME_LEN);

	zassert_not_null(task);
	mrbc_set_task_name(task, task_name);
	zassert_equal(strlen(task->name), name_length);
	zassert_mem_equal(task->name, task_name, name_length);
	zassert_equal_ptr(mrbc_find_task(task->name), task);
	int64_t start = k_uptime_get();

	zassert_equal(mrbc_run(), 0);
	int64_t elapsed = k_uptime_get() - start;

	check_timer_result();
	zassert_true(elapsed >= 20 && elapsed < 200, "sleep_ms took %lld ms", (long long)elapsed);

	const mrbc_value *float_result = mrbc_get_global(mrbc_search_symid("$float_result"));

	zassert_not_null(float_result);
	zassert_equal(mrbc_type(*float_result), MRBC_TT_FLOAT);
	zassert_true(mrbc_float(*float_result) == (mrbc_float_t)3.25);

	const mrbc_value *error = mrbc_get_global(mrbc_search_symid("$trace_result"));

	zassert_not_null(error);
	zassert_equal(mrbc_type(*error), MRBC_TT_EXCEPTION);
	zassert_equal(error->exception->call_nest[0], mrbc_search_symid("trace_inner"));
#if MRBC_EXCEPTION_CALL_NEST_LEVEL > 1
	zassert_equal(error->exception->call_nest[1], mrbc_search_symid("trace_outer"));
#endif

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
#else
K_THREAD_STACK_DEFINE(scheduler_stack, CONFIG_MAIN_STACK_SIZE);
static struct k_thread scheduler_thread;

static void run_scheduler(void *unused1, void *unused2, void *unused3)
{
	ARG_UNUSED(unused1);
	ARG_UNUSED(unused2);
	ARG_UNUSED(unused3);

	mrbc_init(pool, sizeof(pool));
	if (mrbc_create_task(timer_mrb, NULL) != NULL) {
		mrbc_run();
	}
}

ZTEST(mrubyc, test_scheduler_stays_idle_after_task_finishes)
{
	k_tid_t thread = k_thread_create(&scheduler_thread, scheduler_stack,
					K_THREAD_STACK_SIZEOF(scheduler_stack), run_scheduler,
					NULL, NULL, NULL, K_PRIO_PREEMPT(1), 0, K_NO_WAIT);
	int result = k_thread_join(thread, K_MSEC(200));

	k_thread_abort(thread);
	zassert_equal(result, -EAGAIN, "Scheduler returned before the join timed out");
	check_timer_result();
}
#endif

ZTEST_SUITE(mrubyc, NULL, NULL, NULL, NULL, NULL);
