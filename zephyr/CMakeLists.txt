# SPDX-License-Identifier: BSD-3-Clause

if(NOT CONFIG_MRUBYC)
  return()
endif()

set(runtime_dir "${CMAKE_CURRENT_LIST_DIR}/../vendor/mrubyc/src")
set(hal_dir "${CMAKE_CURRENT_LIST_DIR}/hal")

math(EXPR tick_remainder "${CONFIG_SYS_CLOCK_TICKS_PER_SEC} % 1000")
if(NOT tick_remainder EQUAL 0)
  message(STATUS
    "mruby/c: Zephyr rounds the 1 ms HAL timer to whole ticks at ${CONFIG_SYS_CLOCK_TICKS_PER_SEC} Hz")
endif()

if(CONFIG_MRUBYC_FLOAT_SINGLE)
  set(float_mode 1)
else()
  set(float_mode 2)
endif()

zephyr_interface_library_named(MRUBYC_API)
target_include_directories(MRUBYC_API INTERFACE "${runtime_dir}" "${hal_dir}")
target_compile_definitions(MRUBYC_API INTERFACE
  MAX_VM_COUNT=${CONFIG_MRUBYC_MAX_VM_COUNT}
  MAX_REGS_SIZE=${CONFIG_MRUBYC_MAX_REGS_SIZE}
  MAX_SYMBOLS_COUNT=${CONFIG_MRUBYC_MAX_SYMBOLS_COUNT}
  MRBC_USE_FLOAT=${float_mode}
  MRBC_USE_MATH=$<BOOL:${CONFIG_MRUBYC_USE_MATH}>
  MRBC_USE_STRING_UTF8=$<BOOL:${CONFIG_MRUBYC_USE_STRING_UTF8}>
  MRBC_USE_UNICODE_CASE=$<BOOL:${CONFIG_MRUBYC_USE_UNICODE_CASE}>
  $<$<BOOL:${CONFIG_MRUBYC_INT64}>:MRBC_INT64=1>
  $<$<BOOL:${CONFIG_MRUBYC_SUPPORT_OP_EXT}>:MRBC_SUPPORT_OP_EXT=1>
)
# RITE bytecode can contain halfwords and words at unaligned offsets.
if(CONFIG_ARMV6_M_ARMV8_M_BASELINE OR CONFIG_TRAP_UNALIGNED_ACCESS)
  target_compile_definitions(MRUBYC_API INTERFACE MRBC_REQUIRE_32BIT_ALIGNMENT)
endif()
if(CONFIG_TRAP_UNALIGNED_ACCESS)
  target_compile_options(MRUBYC_API INTERFACE -mno-unaligned-access)
endif()
# The native simulator runner already links libm in its final link step.
if(CONFIG_MRUBYC_USE_MATH AND CONFIG_NATIVE_LIBC AND NOT CONFIG_NATIVE_LIBRARY)
  target_link_libraries(MRUBYC_API INTERFACE m)
endif()

zephyr_library_named(mrubyc)
zephyr_library_link_libraries(MRUBYC_API)

if(CONFIG_MRUBYC_FLOAT_SINGLE)
  # Upstream forwards float values to variadic functions, which accept double.
  zephyr_library_compile_options(-Wno-double-promotion)
endif()

if(CONFIG_MRUBYC_USE_MATH)
  # The upstream Math module uses M_E and M_PI, which are outside C99.
  set_source_files_properties("${runtime_dir}/c_math.c" PROPERTIES COMPILE_DEFINITIONS _DEFAULT_SOURCE)
endif()

# Keep this list in sync with the upstream runtime Makefile.
zephyr_library_sources(
  "${runtime_dir}/alloc.c"
  "${runtime_dir}/c_array.c"
  "${runtime_dir}/c_hash.c"
  "${runtime_dir}/c_math.c"
  "${runtime_dir}/c_numeric.c"
  "${runtime_dir}/c_object.c"
  "${runtime_dir}/c_proc.c"
  "${runtime_dir}/c_range.c"
  "${runtime_dir}/c_string.c"
  "${runtime_dir}/class.c"
  "${runtime_dir}/console.c"
  "${runtime_dir}/error.c"
  "${runtime_dir}/global.c"
  "${runtime_dir}/keyvalue.c"
  "${runtime_dir}/load.c"
  "${runtime_dir}/mrblib.c"
  "${runtime_dir}/rrt0.c"
  "${runtime_dir}/symbol.c"
  "${runtime_dir}/value.c"
  "${runtime_dir}/vm.c"
  "${hal_dir}/hal.c"
)
