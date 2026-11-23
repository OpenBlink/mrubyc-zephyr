# Writing mruby/c applications on Zephyr

Use Ruby for application policy, state machines, and data processing. Use
a C binding for access to Zephyr drivers and other native services. Keep
the C calls short so the owning thread can schedule other Ruby tasks.

- [Build and run a recipe](#build-and-run-a-recipe)
- [Write and port Ruby](#write-and-port-ruby)
- [Schedule work and recover from failures](#schedule-work-and-recover-from-failures)
- [Bind Ruby to C](#bind-ruby-to-c)
- [Design peripheral I/O](#design-peripheral-io)
- [Measure and verify](#measure-and-verify)

## Build and run a recipe

These examples target the bundled mruby/c 4.0.0 runtime. Build mrbc 4.0.0
using the [host compiler instructions](../README.md#run-your-own-ruby-code),
add the directory containing `mrbc` to your `PATH`, and confirm the
version with `mrbc --version`.

From the west workspace in the [Hello quick start](../README.md#run-a-sample),
copy the sample and save one example in the copy as `hello.rb`. The sample
configuration enables mruby/c and Zephyr console output. To work from an
unreleased branch, pass its name to `west init --mr`.

```sh
cp -R modules/lib/mrubyc-zephyr/samples/hello ruby-recipe
# Replace ruby-recipe/hello.rb with the Ruby example before compiling.
mrbc -Bhello_mrb --remove-lv -o ruby-recipe/hello_mrb.h ruby-recipe/hello.rb
ZEPHYR_TOOLCHAIN_VARIANT=host west build -b native_sim/native -d build/ruby-recipe ruby-recipe
build/ruby-recipe/zephyr/zephyr.exe -stop_at=1
```

Regenerate the header after changing the script. For applications with
multiple scripts, use a distinct C symbol for each bytecode array and see
the [CMake embedding instructions](../README.md#run-your-own-ruby-code).
The firmware expects RITE0400 bytecode. Check the console output and the
final `mruby/c finished` line. Compilation alone leaves runtime method
availability and error paths untested. Build and embed scripts from
trusted sources. [Runtime limitations](../README.md#known-limitations)

## Write and port Ruby

### Validate input and handle expected errors

Check the input contract before doing arithmetic or sharing state with
another task. Give values units in their names. Rescue the specific error
where the caller can recover.

```ruby
def interval_ms(value)
  unless value.is_a?(Integer) && value >= 10 && value <= 1000
    raise ArgumentError, "interval must be an Integer from 10 to 1000 ms"
  end

  value
end

puts interval_ms(20)

begin
  interval_ms(nil)
rescue ArgumentError => error
  puts error.message
end
```

This prints `20` and the validation message. Ruby treats `0` and empty
strings as truthy. Check those values explicitly when they have special
meaning. The default Integer width is 32 bits. Validate bounds before
arithmetic and run representative inputs on the target configuration.
See the [known 4.0.0 limitations](../README.md#known-limitations), including
the return-through-`ensure` issue. Rerun cleanup and error paths after
changing the compiler or runtime.

Use `nil` for a missing setting when `false` is a valid choice.
`enabled ||= true` also replaces `false`. An explicit nil check preserves it.
Name durations and measurements with units, use `snake_case` for methods
and variables, and use predicate names such as `ready?` for Boolean results.

```ruby
def default_enabled(value)
  return true if value.nil?
  return value if value == true || value == false

  raise ArgumentError, "enabled must be true or false"
end

raise "lost false" unless default_enabled(false) == false
raise "missing default" unless default_enabled(nil) == true
puts "defaults OK"
```

### Use collections within the pool budget

Use `each` for an action per element, `select` to build a filtered array,
and `map` to build a transformed array. Keep the input bounded when using
methods that allocate results. In a hot loop, combine filtering and work
in one `each` pass if you can consume each result immediately.

```ruby
samples_mv = [300, 640, 910]
valid_mv = samples_mv.select { |mv| mv >= 500 }
valid_mv.each { |mv| puts mv }
```

This prints `640` and `910`. Use fixed names for symbols and keep
open-ended inputs as strings, since interning new symbols consumes
dynamic symbol entries and pool storage. Use `String#bytesize` and
`String#byteslice` for byte-oriented data. For text, enable UTF-8 indexing
for the String methods that support it. Unicode case conversion requires
UTF-8 indexing and covers the Basic Multilingual Plane.

Check available methods in the vendored
[String](../vendor/mrubyc/src/_autogen_class_string.h),
[Array](../vendor/mrubyc/src/_autogen_class_array.h), and
[Hash](../vendor/mrubyc/src/_autogen_class_hash.h) method tables. Ruby
methods also ship in the [embedded Ruby library](../vendor/mrubyc/src/mrblib.c).
Exercise any required methods with the firmware's compiler and
[Kconfig settings](../README.md#configuration).

For configuration hashes, use stable Symbol keys and check `has_key?`
if a missing key must be distinguished from a key holding nil. Avoid
repeated copies or unbounded accumulation of samples in a long-lived
task. A ring with a fixed capacity is useful when the consumer lags.

### Parse bytes and size numbers explicitly

For wire data, check the available length first, then read by byte
offset. This example decodes one unsigned 16-bit big-endian field.

```ruby
def read_u16_be(bytes)
  unless bytes.is_a?(String) && bytes.bytesize == 2
    raise ArgumentError, "expected two bytes"
  end

  (bytes.getbyte(0) << 8) | bytes.getbyte(1)
end

raise "wrong bytes" unless read_u16_be("\x00\x80") == 128
begin
  read_u16_be("")
  raise "accepted empty input"
rescue ArgumentError
  puts "short input rejected"
end
```

The supported Integer widths are 32 bits by default and 64 bits with
`CONFIG_MRUBYC_INT64=y`. Check ranges before multiplying or accumulating
and again before narrowing to a native driver type. Choose a Float
precision that fits the data and enable `CONFIG_MRUBYC_USE_MATH` if using
the Math module. Read the [configuration table](../README.md#configuration)
for libc and encoding dependencies. Measure Unicode text operations in
the configuration that will run them, and keep byte offsets for binary
protocols even if UTF-8 indexing is enabled.

### Check idioms before porting

The following choices apply to mruby/c 4.0.0 with mrbc 4.0.0.
Use the matching toolchain and run representative values through every new
idiom on your target. The C method tables linked above and the upstream
[Ruby library source](https://github.com/mrubyc/mrubyc/tree/release4.0.0/mrblib)
cover different parts of the API.

| When porting | Pattern for this runtime |
| --- | --- |
| `map(&:name)` or chained enumerators | Pass an explicit block to `map` or `each_with_index`. |
| `Array#sum`, `reduce`, or conditional `count` | Accumulate from an explicit initial value inside `each`. Use a separate counter for matches. |
| General `case/in` matching | Use `case/when` for simple alternatives. Check type, shape, and keys explicitly before destructuring optional input. Exercise the non-match path. |
| Arrow lambdas and `lambda` | Use a direct block or `Proc.new` where Proc call semantics fit. Recheck arity and nonlocal control flow. |
| `Struct`, open classes, and metaprogramming | Prefer small classes with explicit methods and accessors. Verify the entry points the application calls. |
| `private` or `protected` as an access boundary | Enforce privileged operations in the native API. The fixed runtime can accept external calls to methods marked private or protected. |
| General-purpose `pack` or `unpack` for packets | Write and test fixed-width field readers and writers with an explicit byte order. |
| RBS declarations | Keep them with the application as interface documentation or host tooling input. Validate arguments where Ruby crosses into C. |

## Schedule work and recover from failures

### Let other Ruby tasks run while waiting

Use `sleep_ms` for a timed wait in a Ruby task. The scheduler can run
another Ruby task during that wait. This example produces three updates
and then finishes.

```ruby
3.times do |step|
  puts "step #{step}"
  sleep_ms 20
end
```

The [two-task sample](../samples/vm_tasks) shows one task printing
`a:start`, yielding while it sleeps, and printing `a:done` after the
second task prints `b:ran`. Ruby tasks share one Zephyr thread, and the HAL
timer drives the scheduler. Keep C callbacks short and bounded since
they hold that thread until they return. Check the configured mruby/c
tick period and Zephyr system clock when timing matters.

Build periodic work as finite steps. Define who owns shared state and
where each task may yield. Test ordering with two tasks when changing
priorities, sleeps, or timer settings. `Task.pass` lets the scheduler
choose another runnable task. Use a timed sleep when waiting for an
event, so a task can leave the ready queue. The scheduler and the Zephyr
thread remain separate layers. [Scheduler](../vendor/mrubyc/src/rrt0.c) ·
[HAL](../zephyr/hal/hal.c)

| Task concern | Application contract |
| --- | --- |
| Start from C | Call `mrbc_create_task(bytecode, NULL)` for each script after initialization. Check for NULL before starting `mrbc_run()`. Give each script its own C array and keep its bytecode alive and unchanged for every task or Ruby definition that refers to it. |
| Create from Ruby | `Task.create(bytecode)` takes a String containing compiled bytecode and returns a dormant task. `task.run` makes it runnable. Prefer static C registration for fixed firmware tasks. Keep dynamically loaded bytes trusted and immutable. |
| Inspect tasks | Use `Task.current`, `Task.list`, and `task.status`. `READY` includes runnable work, `WAITING` reports a reason such as sleep, `SUSPENDED` requires a resume, and `DORMANT` includes stopped or unstarted tasks. |
| Name and prioritize | Set `task.name` within `CONFIG_MRUBYC_TASK_NAME_LEN` bytes. Priorities run from 0 (highest) to 255 (lowest). Ruby priority is independent of Zephyr thread priority. |
| Set timing | The default slice is 10 mruby/c ticks at 1 ms per tick. Sleep and preemption depend on the Zephyr clock and when the VM can next run. Use the target to measure actual wakeup latency. |
| Finish | With `CONFIG_MRUBYC_SCHEDULER_EXIT=y`, `mrbc_run()` returns once the scheduler has no ready, waiting, or suspended tasks. A suspended task can keep it alive. Budget VM count and per-VM registers when adding tasks. |

Each successful `Task.create` reserves a VM ID, which remains allocated
after the task becomes dormant. The Ruby `Task` API has no delete method in
4.0.0. Use a fixed set of long-lived tasks for recurring work to avoid
exhausting `CONFIG_MRUBYC_MAX_VM_COUNT`. For tasks whose lifetime is
managed in C, `mrbc_delete_task()` closes a dormant task's VM. Release
references to the task before deleting it, including any Ruby handles.

### Release resources on success and error

Use a block to scope ownership and `ensure` to release the resource.
Use a narrow `rescue` at the point where the caller can retry or choose a
fallback. Derive application errors from `StandardError`. Keep cleanup
independent of arguments to a method returning through `ensure`, given
the upstream 4.0.0 limitation. The [I/O example](#exercise-waiting-with-a-fake)
tests release after both completion and a timeout.

An unhandled exception normally appears at VM shutdown while other Ruby
tasks can continue. `CONFIG_MRUBYC_PANIC_ON_UNHANDLED_EXCEPTION=y`
changes that shutdown policy to a Zephyr panic. Ruby-created
`Task.create` tasks retain their VMs on stop and skip the shutdown hook.
Keep application-level recovery in the application, and release locks or
devices before terminating a task. [Fatal hook tests](../tests/fatal_hooks/testcase.yaml)

## Bind Ruby to C

### Register a C method

Use a C binding when Ruby needs a native operation. This method accepts
one integer percentage and returns that value after checking the range.
It shows the argument contract without requiring a board or driver.

```c
static void check_percent(mrbc_vm *vm, mrbc_value v[], int argc)
{
    if (argc != 1) {
        mrbc_raise(vm, MRBC_CLASS(ArgumentError), "expected one argument");
        return;
    }

    if (mrbc_type(v[1]) != MRBC_TT_INTEGER) {
        mrbc_raise(vm, MRBC_CLASS(TypeError), "expected an Integer");
        return;
    }

    mrbc_int_t percent = mrbc_integer(v[1]);
    if (percent < 0 || percent > 100) {
        mrbc_raise(vm, MRBC_CLASS(ArgumentError), "expected 0 to 100");
        return;
    }

    SET_INT_RETURN(percent);
}
```

In `ruby-recipe/main.c`, place the function above `main()`. Add
`mrbc_define_method(NULL, MRBC_CLASS(Object), "check_percent", check_percent);`
after `mrbc_init()` and before `mrbc_create_task()`. Compile and run this
script using the commands above.

```ruby
puts check_percent(25)

begin
  check_percent(101)
rescue ArgumentError => error
  puts error.message
end
```

This prints `25` and `expected 0 to 100`.

The callback receives the receiver in `v[0]` and positional arguments in
`v[1]` through `v[argc]` for this call. Raise and return before any
side effects on invalid input. `SET_INT_RETURN` releases the receiver
value in the return slot and writes the result. When handling heap
objects in a binding, check whether the API borrows, retains, or takes
ownership of each value. Inspect
[`value.h`](../vendor/mrubyc/src/value.h) and the relevant class API.

Link separate Zephyr libraries that include `mrubyc.h` with
`zephyr_library_link_libraries(MRUBYC_API)` so they inherit the runtime's
definitions. Keep VM access on the owning Zephyr thread. For ISR or
other-thread input, pass data to that thread through an appropriate
Zephyr synchronization primitive.

### Expose a class or a module

Register application-wide classes with
`mrbc_define_class(NULL, "Device", MRBC_CLASS(Object))` after `mrbc_init()`.
Use `mrbc_define_module` for a module and the `*_under` functions to
register under a namespace. Use `mrbc_define_method` for the callbacks.
Class and instance receivers can reach the same method table in 4.0.0.
Check the receiver's tag and class identity for a class-only method.
An `initialize` callback sets up the receiver in place.
[Class and method API](../vendor/mrubyc/src/class.h)

### Validate positional arguments, keywords, and results

Check arity before accessing arguments. Check the tag with `mrbc_type`
before reading the payload with `mrbc_integer` or `mrbc_float`. Some
argument helpers coerce values. For example, `mrbc_arg_i` accepts a Float,
so an Integer-only method needs an explicit tag check. Check the range
before narrowing to a driver type. For a strict Boolean, accept the
`MRBC_TT_TRUE` and `MRBC_TT_FALSE` tags. For a String, check the byte
length with `mrbc_string_size` and use `mrbc_string_cstr` for a borrowed
pointer. Use the explicit length for data that can contain NUL bytes.

In ordinary dispatch, keywords are passed as a Hash after the positional
arguments when present. The block slot follows the keywords, or the
positional arguments if there are no keywords. Use `MRBC_KW_ARG` to
extract named arguments, `MRBC_KW_ISVALID` to test presence, and
`MRBC_KW_END` to reject unknown keys. An omitted keyword is EMPTY.
An explicitly supplied Ruby `nil` is present. Use `MRBC_KW_DICT` when
accepting arbitrary remaining keywords, and release the Hash it
hands to the callback once you have finished with it.

Register the following method on Object as `poll_period` using the same
steps as for `check_percent`.

```c
static void poll_period(mrbc_vm *vm, mrbc_value v[], int argc)
{
    if (argc != 0) {
        mrbc_raise(vm, MRBC_CLASS(ArgumentError), "expected keywords only");
        return;
    }

    MRBC_KW_ARG(period_ms);
    if (!MRBC_KW_MANDATORY(period_ms) || !MRBC_KW_END()) {
        goto cleanup;
    }

    if (mrbc_type(period_ms) != MRBC_TT_INTEGER) {
        mrbc_raise(vm, MRBC_CLASS(TypeError), "period_ms must be an Integer");
        goto cleanup;
    }

    mrbc_int_t period = mrbc_integer(period_ms);
    if (period < 1 || period > 1000) {
        mrbc_raisef(vm, MRBC_CLASS(RangeError),
                    "%s must be in 1..1000", "period_ms");
        goto cleanup;
    }

    SET_INT_RETURN(period);

cleanup:
    MRBC_KW_DELETE(period_ms);
}
```

```ruby
raise "wrong period" unless poll_period(period_ms: 20) == 20

begin
  poll_period(period_ms: nil)
  raise "accepted nil"
rescue TypeError
  puts "nil rejected"
end

begin
  poll_period(period_ms: 20, extra: "unexpected")
  raise "accepted an unknown keyword"
rescue ArgumentError
  puts "unknown keyword rejected"
end

begin
  poll_period
  raise "accepted a missing keyword"
rescue ArgumentError
  puts "missing keyword rejected"
end

begin
  poll_period(period_ms: 0)
  raise "accepted a zero period"
rescue RangeError
  puts "zero period rejected"
end

raise "wrong period after errors" unless poll_period(period_ms: 1000) == 1000
```

`mrbc_raise` and `mrbc_raisef` set the VM exception and return to C.
On error, return immediately or jump to cleanup and return from there.
Keep native calls and success results on the success path. On the Ruby
side, catch the specific exceptions the caller can handle.
[Argument, keyword, and return macros](../vendor/mrubyc/src/value.h) ·
[Exception API](../vendor/mrubyc/src/error.h)

### Manage ownership and cleanup

| Situation | What the binding does |
| --- | --- |
| Return an immediate value | Use `SET_INT_RETURN`, `SET_BOOL_RETURN`, or `SET_NIL_RETURN`. Each releases the old return slot and assigns the result. End the C callback when finished. |
| Return a newly allocated String, Array, or Hash | Transfer the owned reference using `SET_RETURN`. Release partially built values on error. |
| Return or store a borrowed heap value | Call `mrbc_incref` to obtain an owned reference before storing it or transferring it to the return slot. This also applies to `SET_RETURN(v[0])`, which releases the old receiver. |
| Drop an owned reference | Use `mrbc_decref`, or `mrbc_decref_empty` when the slot also needs to be cleared. For persistent values, release the saved reference on replacement, cancellation, or shutdown. |
| Use a pointer to String bytes | Read within the callback while its owner is alive. Copy into a bounded native buffer if a driver or worker will use the bytes later. Keep that buffer alive through completion, including late completion after a timeout. |
| Insert or retrieve a container member | Check the individual C API. Array and Hash getters borrow. Successful Array/Hash inserts take ownership of the supplied references. Instance-variable APIs have different retain behavior. |

Treat callback arguments as borrowed. `SET_RETURN` transfers an owned
value and continues executing C. Avoid cycles of owned references and
keep retained Ruby values in their originating runtime. A Proc can also
depend on its captured frame or VM, even when retained.
Call Ruby blocks from a Ruby wrapper. The 4.0.0 `mrbc_send` API supports
synchronous dispatch to C methods and reports an error for Ruby IREP
methods. [Value API](../vendor/mrubyc/src/value.h) ·
[Method dispatch](../vendor/mrubyc/src/class.c)

Use `mrbc_instance_new` to allocate an instance with inline native data
when implementing a native constructor. `Object#dup` copies the class
and instance variables but allocates zero bytes of inline data. Verify
the receiver's tag, class, and native registration before accessing its
data. For example, keep a bounded table keyed by the identity of each
instance initialized by the binding. A copy will have a different identity.
[Object duplication](../vendor/mrubyc/src/c_object.c)

Make explicit close idempotent and invalidate the native registration
when releasing the resource. Apply the same validity check in a destructor.
With `CONFIG_MRUBYC_INSTANCE_DESTRUCTOR=y`, `mrbc_define_destructor`
can register a fallback for ordinary instance destruction. The
destructor receives the value. Close native resources on the application
shutdown path as well, since releasing a whole pool skips per-object
destructors.

Use the `mrbc_*` value APIs so a separate library can build with
`CONFIG_MRUBYC_DISABLE_LEGACY_VALUE_MACROS=y`. Link to `MRUBYC_API` to
share the runtime's configuration, including type widths and destructor
support. [Module build configuration](../zephyr/CMakeLists.txt)

## Design peripheral I/O

The module supplies the VM, scheduler, and Zephyr HAL. Add peripheral
classes in an application library with its own Zephyr driver and
devicetree configuration. Use the
[Common I/O API Guidelines index][mpig] to design portable GPIO, ADC,
PWM, UART, I2C, and SPI bindings. The index links the English and Japanese
contracts at the pinned revision.

### Match the interface contract

For each constructor and method, record the class or instance receiver,
arity, keyword defaults, argument types and units, and return type.
Document standard APIs, optional methods, and application extensions.
Keep implementation-specific operations under distinct names and
document how to discover or configure them.

| Interface | Portable contract to preserve | Firmware choices to document |
| --- | --- | --- |
| [GPIO][mpig-gpio] | Class operations such as `read_at` and instance operations such as `read` and `setmode`. Preserve `IN`, `OUT`, `HIGH_Z`, `PULL_UP`, `PULL_DOWN`, and `OPEN_DRAIN` modes and flags. For single-pin digital bindings, use Integer 0 or 1 for levels and Boolean predicates. | Map the Ruby pin to a Zephyr controller and pin. Define active-low behavior, partial mode updates, allowed flag combinations, sharing, and release. |
| [ADC][mpig-adc] | `read_raw` returns Integer counts. `read_voltage` and `read` return Float volts. | Specify channels, resolution, reference voltage, calibration, and supported parameters. |
| [PWM][mpig-pwm] | Frequency is in Hz, duty is a percentage, and `period_us` and `pulse_width_us` take microseconds. Frequency 0 stops output. Starting with a constructor frequency and omitted duty uses 50%. | Record ranges and rounding, polarity, shared timer constraints, and pin state on stop. |
| [UART][mpig-uart] | `read(n)` waits for the requested length. `gets` waits for an LF-terminated line. `write` returns a byte count and `puts` returns nil. `flush` waits for TX to drain. Preserve the parity and flow-control constants `NONE`, `EVEN`, `ODD`, and `RTSCTS` when supported. | Describe buffer sizes, line overflow, what counts as transmitted versus queued, and how the task waits. Use `bytes_available`, `bytes_to_write`, and `can_read_line` to report readiness. Document buffer clearing and `send_break` in seconds. |
| [I2C][mpig-i2c] | Use 7-bit addresses and Hz with a 100 kHz default. `read` returns a String and can return fewer bytes after a NAK. `write` returns a byte count. Optional data preceding a read uses a repeated START. | Record STOP and error behavior and keep a combined write/read indivisible to other clients. `send_start`, `send_restart`, `send_stop`, `raw_read`, and `raw_write` are optional low-level operations. |
| [SPI][mpig-spi] | Master transfers use 8-bit words, modes 0–3, `MSB_FIRST` or `LSB_FIRST`, and a 1 MHz default. `read` sends zero bytes as clocks are generated, `write` returns nil, and `transfer` returns the bytes received during transmit and any additional clocks. | Specify the chip-select owner, polarity, and timing across a transaction, including errors. Document the supported parameters for `setmode`. |

Peripheral contract summary adapted from the
[mruby and mruby/c Common I/O API Guidelines][mpig] at revision
`514497cfe184edda842f4410cd0eaee1acbdfb9c`. Copyright © 2023 mruby forum
committee, Kyushu Institute of Technology, and Shimane IT Open-Innovation
Center. Source licensed under [CC BY 4.0][cc-by]. The summary condenses
the APIs and adds Zephyr-specific design choices.

### Bound driver work and define failure behavior

- Define who owns each physical controller, channel, pin, and transaction.
  Specify which handles may share them and how close, cancel, and task
  shutdown free the resource.
- Choose a maximum byte length, queue capacity, and buffer lifetime.
  Validate configuration, counts, types, and ranges before allocation or
  submission. Specify behavior for empty data, nil, unsupported
  settings, and full queues.
- Define busy, timeout, cancellation, partial transfer, and late completion
  in terms of return values or exceptions. Preserve standard return types
  and units. For example, use a distinct API for a nonblocking UART read
  that returns nil when no data is available.
- Use a monotonic deadline, with wrap-safe arithmetic for the chosen
  counter, to bound application-specific waits. Account for tick rounding
  and the maximum time spent inside each driver call. Keep buffers owned
  by an in-flight transfer until completion or confirmed cancellation,
  even after an application times out.
- Break long waits into short native operations to submit work, check
  status, and collect results. A Ruby wrapper can sleep between status
  checks while preserving a method's documented behavior of waiting for
  completion. Give asynchronous extensions explicit cancellation and
  completion contracts.
- Use a bounded ISR-safe notification such as a Zephyr message queue
  with `K_NO_WAIT` or an atomic flag. Choose an overflow policy and
  synchronize multiple producers. Drain notifications and perform VM
  calls, Ruby allocations, reference counting, and error reporting on
  the Ruby-owning thread. A C callback that waits on a Zephyr primitive
  also holds up the other Ruby tasks.

### Exercise waiting with a fake

Use a fake backend with an operation log and controlled completion.
Before testing pins or a bus, use it to check accepted configuration,
bytes and ordering, capacity, delay, partial transfers, cancellation,
release on failure, and late completion. Assert each rejection leaves
the backend unchanged. Include wrong types, missing arguments, nil,
empty input, out-of-range values, and a valid request after each error.

This application-level recipe uses a Ruby fake and an attempt limit to
test completion and cleanup. A production timeout should use an
elapsed-time deadline.

```ruby
class TransferTimeout < StandardError
end

class FakeTransfer
  attr_reader :closed

  def initialize(ready_after)
    @remaining = ready_after
    @closed = false
  end

  def poll
    return "OK" if @remaining == 0

    @remaining -= 1
    nil
  end

  def with_transfer
    yield
  ensure
    close
  end

  def close
    @closed = true
  end
end

def read_with_poll_limit(transfer)
  attempts = 0
  while attempts < 3
    result = transfer.poll
    return result unless result.nil?

    attempts += 1
    sleep_ms 2 if attempts < 3
  end
  raise TransferTimeout, "transfer timed out"
end

transfer = FakeTransfer.new(1)
result = transfer.with_transfer { read_with_poll_limit(transfer) }
raise "wrong result" unless result == "OK"
raise "transfer still open" unless transfer.closed

transfer = FakeTransfer.new(5)
begin
  transfer.with_transfer { read_with_poll_limit(transfer) }
  raise "expected timeout"
rescue TransferTimeout
  raise "transfer still open" unless transfer.closed
  puts "timeout closed the transfer"
end
```

## Measure and verify

Check the firmware's Flash and static RAM at link time. Measure memory
pool use during a representative workload and inspect the Zephyr thread
stack separately. Start with the [footprint comparison and reproduction
steps](../README.md#memory-footprint-rom-and-ram). If you change a
[Kconfig setting](../README.md#configuration), check the resolved
`.config`, rebuild, and rerun the script with the same board and compiler.

- Use `CONFIG_MRUBYC_ALLOC_PROFILING` for allocator high-water data.
  Record steady-state use as well as bursts during task creation,
  exceptions, and transfers. Tune pool size, VM count, register count,
  and symbol capacity from the resulting workload.
- Run recipes on `native_sim/native` and `qemu_cortex_m3`. Check
  expected output and that `mrbc_run()` exits. Use the
  [repository test instructions](../README.md#tests-and-contributions) for the module's
  existing tests.
- Verify a binding's driver behavior on the actual board. Timing,
  pin polarity, interrupts, DMA, and resource sharing depend on the
  device and its configuration.

[mpig]: https://github.com/mruby/microcontroller-peripheral-interface-guide/tree/514497cfe184edda842f4410cd0eaee1acbdfb9c
[mpig-gpio]: https://github.com/mruby/microcontroller-peripheral-interface-guide/blob/514497cfe184edda842f4410cd0eaee1acbdfb9c/mruby_io_GPIO_en.md
[mpig-adc]: https://github.com/mruby/microcontroller-peripheral-interface-guide/blob/514497cfe184edda842f4410cd0eaee1acbdfb9c/mruby_io_ADC_en.md
[mpig-pwm]: https://github.com/mruby/microcontroller-peripheral-interface-guide/blob/514497cfe184edda842f4410cd0eaee1acbdfb9c/mruby_io_PWM_en.md
[mpig-uart]: https://github.com/mruby/microcontroller-peripheral-interface-guide/blob/514497cfe184edda842f4410cd0eaee1acbdfb9c/mruby_io_UART_en.md
[mpig-i2c]: https://github.com/mruby/microcontroller-peripheral-interface-guide/blob/514497cfe184edda842f4410cd0eaee1acbdfb9c/mruby_io_I2C_en.md
[mpig-spi]: https://github.com/mruby/microcontroller-peripheral-interface-guide/blob/514497cfe184edda842f4410cd0eaee1acbdfb9c/mruby_io_SPI_en.md
[cc-by]: https://creativecommons.org/licenses/by/4.0/
