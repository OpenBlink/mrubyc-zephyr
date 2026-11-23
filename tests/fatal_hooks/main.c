/* SPDX-License-Identifier: BSD-3-Clause */

#include <mrubyc.h>
#include <stdarg.h>
#include <string.h>
#include <zephyr/fatal.h>
#include <zephyr/sys/printk.h>

#ifdef CONFIG_ARCH_POSIX
#include <posix_board_if.h>
#endif

#ifndef TRIGGER_OOM
#include "exception_mrb.h"
#endif
#ifdef USE_DYNAMIC_TASK
#include "dynamic_task_mrb.h"
#endif

#ifdef TRIGGER_OOM
static bool saw_pool_stats;

/* Observe pool diagnostics so the default test can check their absence. */
void __wrap_printk(const char *format, ...)
{
	va_list args;
	static const char prefix[] = "mruby/c pool: ";

	if (strncmp(format, prefix, sizeof(prefix) - 1) == 0) {
		saw_pool_stats = true;
	}
	va_start(args, format);
	vprintk(format, args);
	va_end(args);
}
#endif

void k_sys_fatal_error_handler(unsigned int reason, const struct arch_esf *esf)
{
	ARG_UNUSED(esf);

	if (reason == K_ERR_KERNEL_PANIC) {
#ifdef TRIGGER_OOM
		printk("Pool statistics printed: %s\n", saw_pool_stats ? "yes" : "no");
#endif
		printk("Caught Zephyr kernel panic\n");
#ifdef CONFIG_ARCH_POSIX
		/* Report the expected panic as a successful simulator exit. */
		posix_exit(0);
#endif
	}
	k_fatal_halt(reason);
}

int main(void)
{
	static _Alignas(8) uint8_t pool[16 * 1024];

	mrbc_init(pool, sizeof(pool));
#ifdef TRIGGER_OOM
	/* The pool header and existing allocations leave less than this size. */
	mrbc_raw_alloc(sizeof(pool));
#else
#ifdef USE_DYNAMIC_TASK
	mrbc_value script = mrbc_string_new(NULL, exception_mrb, sizeof(exception_mrb));

	if (mrbc_set_global(mrbc_str_to_symid("$task_bytecode"), &script) != 0) {
		return 1;
	}
	const uint8_t *bytecode = dynamic_task_mrb;
#else
	const uint8_t *bytecode = exception_mrb;
#endif
	if (mrbc_create_task(bytecode, NULL) == NULL) {
		return 1;
	}
	if (mrbc_run() == 2) {
		printk("Scheduler returned the unhandled exception status\n");
	}
#endif
	return 0;
}
