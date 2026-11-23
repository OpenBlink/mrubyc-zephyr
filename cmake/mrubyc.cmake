# SPDX-License-Identifier: BSD-3-Clause

include_guard(GLOBAL)

function(mrubyc_check_compiler compiler)
  set(runtime_dir "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../vendor/mrubyc/src")
  file(STRINGS "${runtime_dir}/VERSION" version_line REGEX "^MRUBY_VERSION")
  string(REGEX MATCH "[0-9]+[.][0-9]+[.][0-9]+" expected "${version_line}")
  if(NOT expected)
    message(FATAL_ERROR "mruby/c: MRUBY_VERSION is missing from the bundled runtime")
  endif()

  execute_process(COMMAND "${compiler}" --version
    RESULT_VARIABLE status OUTPUT_VARIABLE version ERROR_VARIABLE error
    OUTPUT_STRIP_TRAILING_WHITESPACE)
  string(REGEX MATCH "^mruby ([0-9]+[.][0-9]+[.][0-9]+)( |$)" match "${version}")
  if(NOT status STREQUAL "0" OR NOT match OR NOT CMAKE_MATCH_1 STREQUAL expected)
    message(FATAL_ERROR
      "mruby/c: expected mrbc ${expected} from '${compiler}', got '${version}${error}' (${status})")
  endif()
endfunction()

function(mrubyc_embed_script target symbol source)
  if(NOT TARGET "${target}" OR NOT CONFIG_MRUBYC)
    message(FATAL_ERROR "mrubyc_embed_script requires an existing target and CONFIG_MRUBYC=y")
  endif()
  if(NOT symbol MATCHES "^[a-zA-Z_][a-zA-Z_0-9]*$")
    message(FATAL_ERROR "mruby/c: '${symbol}' is not a C identifier")
  endif()

  cmake_path(ABSOLUTE_PATH source BASE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" NORMALIZE)
  if(NOT source MATCHES "[.]rb$" OR NOT EXISTS "${source}" OR IS_DIRECTORY "${source}")
    message(FATAL_ERROR "mruby/c: expected an existing .rb source file, got '${source}'")
  endif()
  get_property(embedded TARGET "${target}" PROPERTY MRUBYC_EMBEDDED_SYMBOLS)
  if(symbol IN_LIST embedded)
    message(FATAL_ERROR "mruby/c: duplicate bytecode symbol '${symbol}' on target '${target}'")
  endif()

  if(MRUBYC_MRBC)
    set(compiler "${MRUBYC_MRBC}")
    if(NOT IS_ABSOLUTE "${compiler}")
      message(FATAL_ERROR "mruby/c: MRUBYC_MRBC must be an absolute path")
    endif()
  else()
    find_program(_MRUBYC_MRBC_FROM_PATH NAMES mrbc NO_CMAKE_FIND_ROOT_PATH)
    set(compiler "${_MRUBYC_MRBC_FROM_PATH}")
    unset(_MRUBYC_MRBC_FROM_PATH CACHE)
    if(NOT compiler)
      message(FATAL_ERROR "mruby/c: install the host mrbc or set MRUBYC_MRBC")
    endif()
  endif()
  mrubyc_check_compiler("${compiler}")

  set(out_dir "${CMAKE_CURRENT_BINARY_DIR}/mrubyc/${target}")
  set(header "${out_dir}/${symbol}.h")
  set(driver "${CMAKE_CURRENT_FUNCTION_LIST_FILE}")
  set(runtime_dir "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../vendor/mrubyc/src")
  add_custom_command(
    OUTPUT "${header}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${out_dir}"
    COMMAND "${CMAKE_COMMAND}" "-DCOMPILER=${compiler}" "-DSOURCE=${source}"
            "-DSYMBOL=${symbol}" "-DHEADER=${header}" -P "${driver}"
    DEPENDS "${source}" "${compiler}" "${driver}" "${runtime_dir}/VERSION" "${runtime_dir}/load.c"
    VERBATIM
  )
  set_property(TARGET "${target}" APPEND PROPERTY MRUBYC_EMBEDDED_SYMBOLS "${symbol}")
  target_sources("${target}" PRIVATE "${header}")
  target_include_directories("${target}" PRIVATE "${out_dir}")
endfunction()

if(CMAKE_SCRIPT_MODE_FILE STREQUAL CMAKE_CURRENT_LIST_FILE)
  mrubyc_check_compiler("${COMPILER}")
  file(STRINGS "${CMAKE_CURRENT_LIST_DIR}/../vendor/mrubyc/src/load.c" rite_line REGEX
    "RITE_VERSION\\[4\\] = \"[0-9]+\";")
  string(REGEX MATCH "[0-9][0-9][0-9][0-9]" rite_version "${rite_line}")
  if(NOT rite_version)
    message(FATAL_ERROR "mruby/c: RITE_VERSION is missing from the bundled loader")
  endif()
  string(HEX "RITE${rite_version}" rite_hex)
  string(REGEX REPLACE "(..)" "0x\\1," rite_prefix "${rite_hex}")

  set(temporary "${HEADER}.tmp")
  execute_process(COMMAND "${COMPILER}" "-B${SYMBOL}" --remove-lv -o "${temporary}" "${SOURCE}"
    RESULT_VARIABLE status)
  if(NOT status STREQUAL "0")
    file(REMOVE "${temporary}")
    message(FATAL_ERROR "mruby/c: mrbc failed to compile '${SOURCE}' (${status})")
  endif()
  file(READ "${temporary}" signature LIMIT 512)
  if(NOT signature MATCHES "const uint8_t ${SYMBOL}\\[\\] = \\{[ \n\r]*${rite_prefix}")
    file(REMOVE "${temporary}")
    message(FATAL_ERROR "mruby/c: expected bytecode RITE${rite_version} in '${HEADER}'")
  endif()
  file(RENAME "${temporary}" "${HEADER}")
endif()
