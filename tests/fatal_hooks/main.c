/* SPDX-License-Identifier: BSD-3-Clause */

#include <mrubyc.h>
#include <zephyr/fatal.h>
#include <zephyr/sys/printk.h>

#ifdef CONFIG_ARCH_POSIX
#include <posix_board_if.h>
#endif

#ifndef TRIGGER_OOM
#include "exception_mrb.h"
#endif

void k_sys_fatal_error_handler(unsigned int reason, const struct arch_esf *esf)
{
	ARG_UNUSED(esf);

	if (reason == K_ERR_KERNEL_PANIC) {
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
	if (mrbc_create_task(exception_mrb, NULL) == NULL) {
		return 1;
	}
	if (mrbc_run() == 2) {
		printk("Scheduler returned the unhandled exception status\n");
	}
#endif
	return 0;
}
