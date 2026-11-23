# Writing mruby/c applications on Zephyr

These examples target the bundled mruby/c 4.0.0 runtime and mrbc 4.0.0.
From the west workspace in the [Hello quick start](../README.md#run-a-sample),
copy the sample and save one example in the copy as `hello.rb`. The sample
configuration enables mruby/c and Zephyr console output. To work from an
unreleased revision, use its branch or commit in the `west init --mr`
argument and check the compiler version with `mrbc --version`.

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

## Validate input and handle expected errors

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

## Use collections within the pool budget

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

## Let other Ruby tasks run while waiting

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

## Register a C method

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

## Measure before tuning

Check the firmware's Flash and static RAM at link time. Measure memory
pool use during a representative workload and inspect the Zephyr thread
stack separately. Start with the [footprint comparison and reproduction
steps](../README.md#memory-footprint-rom-and-ram). If you change a
[Kconfig setting](../README.md#configuration), check the resolved
`.config`, rebuild, and rerun the script with the same board and compiler.
