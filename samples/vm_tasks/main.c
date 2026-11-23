/* SPDX-License-Identifier: BSD-3-Clause */

#include <stdint.h>
#include <mrubyc.h>
#include <zephyr/sys/printk.h>

#include "task_a_mrb.h"
#include "task_b_mrb.h"

static _Alignas(8) uint8_t pool[24 * 1024];

int main(void)
{
	mrbc_init(pool, sizeof(pool));
	if (mrbc_create_task(task_a_mrb, NULL) == NULL ||
	    mrbc_create_task(task_b_mrb, NULL) == NULL) {
		printk("Unable to load the Ruby tasks\n");
		return 1;
	}

	int result = mrbc_run();

	printk("Scheduler returned %d\n", result);
	return result == 0 ? 0 : 1;
}
