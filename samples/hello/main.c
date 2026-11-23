/* SPDX-License-Identifier: BSD-3-Clause */

#include <stdint.h>
#include <mrubyc.h>
#include <zephyr/sys/printk.h>

#include "hello_mrb.h"

static _Alignas(8) uint8_t pool[16 * 1024];

int main(void)
{
	mrbc_init(pool, sizeof(pool));
	if (mrbc_create_task(hello_mrb, NULL) == NULL) {
		printk("Unable to load the Ruby task\n");
		return 1;
	}
	if (mrbc_run() != 0) {
		printk("Ruby task failed\n");
		return 1;
	}
	printk("mruby/c finished\n");
	return 0;
}
