/* SPDX-License-Identifier: BSD-3-Clause */

#ifndef MRUBYC_ZEPHYR_FATAL_HOOKS_H
#define MRUBYC_ZEPHYR_FATAL_HOOKS_H

#ifdef CONFIG_MRUBYC_OOM_POOL_STATS
#define MRBC_OUT_OF_MEMORY()                                                           \
	do {                                                                           \
		struct MRBC_ALLOC_STATISTICS stats;                                    \
		mrbc_alloc_statistics(&stats);                                         \
		printk("mruby/c pool: total=%u used=%u free=%u bytes\n",                  \
		       stats.total, stats.used, stats.free);                            \
		mrbc_hal_abort("Fatal error: Out of memory.\n");                         \
	} while (0)
#endif

#ifdef CONFIG_MRUBYC_PANIC_ON_UNHANDLED_EXCEPTION
#define MRBC_ABORT_BY_EXCEPTION(vm)                                                    \
	do {                                                                           \
		mrbc_print_vm_exception(vm);                                            \
		mrbc_hal_abort(NULL);                                                  \
	} while (0)
#endif

#endif /* MRUBYC_ZEPHYR_FATAL_HOOKS_H */
