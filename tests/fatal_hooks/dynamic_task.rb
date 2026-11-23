# SPDX-License-Identifier: BSD-3-Clause

task = Task.create($task_bytecode)
task.run
task.join
puts "Dynamic task joined"
