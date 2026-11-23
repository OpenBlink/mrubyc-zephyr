/* SPDX-License-Identifier: BSD-3-Clause */

#include <stdint.h>
#include <mrubyc.h>

#include "task_a_mrb.h"
#include "task_b_mrb.h"

int main(void)
{
	static _Alignas(8) uint8_t pool[24 * 1024];

	mrbc_init(pool, sizeof(pool));
	if (mrbc_create_task(task_a_mrb, NULL) == NULL ||
	    mrbc_create_task(task_b_mrb, NULL) == NULL) {
		return 1;
	}
	return mrbc_run() == 0 ? 0 : 1;
}
