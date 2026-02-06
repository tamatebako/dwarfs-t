#
# Copyright (c) Marcus Holland-Moritz
#
# This file is part of dwarfs.
#
# dwarfs is free software: you can redistribute it and/or modify it under the
# terms of the GNU General Public License as published by the Free Software
# Foundation, either version 3 of the License, or (at your option) any later
# version.
#
# dwarfs is distributed in the hope that it will be useful, but WITHOUT ANY
# WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
# A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along with
# dwarfs.  If not, see <https://www.gnu.org/licenses/>.
#

# Conditional minimum version for tebako compatibility

# ============================================================================
# Include Directories for All Targets
# ============================================================================

foreach(tgt ${LIBDWARFS_TARGETS})
  target_include_directories(${tgt} SYSTEM PRIVATE
    $<BUILD_INTERFACE:$<TARGET_PROPERTY:phmap,INTERFACE_INCLUDE_DIRECTORIES>>
  )
endforeach()

# ============================================================================
# Common Compiler Flags and Settings
# ============================================================================

foreach(tgt ${LIBDWARFS_TARGETS} ${LIBDWARFS_OBJECT_TARGETS}
            dwarfs_test_helpers dwarfs_test_main
            ${BINARY_TARGETS} ${TEST_TARGETS} ${MAIN_TARGETS})
  if(NOT TARGET ${tgt})
    continue()
  endif()

  # Check if target is INTERFACE library
  get_target_property(tgt_type ${tgt} TYPE)
  if(tgt_type STREQUAL "INTERFACE_LIBRARY")
    continue()
  endif()

  set_target_properties(${tgt} PROPERTIES EXPORT_COMPILE_COMMANDS ON)

  # Use Boost::headers instead of Boost::boost
  # vcpkg's boost ports provide Boost::headers for header-only libraries
  # Boost::boost is not provided by vcpkg
  target_link_libraries(${tgt} PUBLIC Boost::headers)

  if(WITH_LIBDWARFS)
    target_include_directories(${tgt} PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
      $<INSTALL_INTERFACE:include>
    )
  else()
    # This is a workaround to *enforce* the include path in case we're in a
    # setup where libdwarfs is installed in a local path but some of its
    # dependencies are installed in a system path *AND* there's a different
    # version of libdwarfs *ALSO* installed in the system path.
    target_include_directories(${tgt} BEFORE PUBLIC ${DWARFS_INCLUDE_DIR})
  endif()

  target_include_directories(${tgt} SYSTEM PUBLIC
    $<BUILD_INTERFACE:$<TARGET_PROPERTY:range-v3::range-v3,INTERFACE_INCLUDE_DIRECTORIES>>
  )

  if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" OR
     "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    target_compile_options(${tgt} PRIVATE -Wall -Wextra -pedantic)
  endif()

  if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    target_compile_options(${tgt} PRIVATE -Wno-stringop-overflow)
  endif()

  if(NOT ${tgt} MATCHES "folly|fsst|thrift")
    if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
      target_compile_options(${tgt} PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:
          -Wold-style-cast
          -Wnon-virtual-dtor
          -Wsuggest-override
          -Wpessimizing-move
        >
        -Wfloat-equal
        -Wcast-align
        -Wpointer-arith
        -Wformat=2
      )
      # if(DWARFS_GIT_BUILD)
      #   target_compile_options(${tgt} PRIVATE -Werror)
      # endif()
    endif()

    if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
      target_compile_options(${tgt} PRIVATE
        -Wnull-dereference  # this is too flaky with GCC
      )
    endif()

    if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
      target_compile_options(${tgt} PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:-Wuseless-cast>
        -Wlogical-op
        -Wduplicated-cond
        -Wduplicated-branches
        -Wmisleading-indentation
      )
      if("${CMAKE_CXX_COMPILER_VERSION}" VERSION_GREATER_EQUAL "14")
        target_compile_options(${tgt} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-Wnrvo -Wno-error=nrvo>)
      endif()
    endif()

    if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
      # See: https://github.com/cppbest-practices/cppbestpractices/blob/master/02-Use_the_Tools_Available.md
      target_compile_options(${tgt} PRIVATE
        /W4 # Baseline reasonable warnings
        # Too noisy:
        # /w14242 # 'identifier': conversion from 'type1' to 'type2', possible loss of data
        /w14254 # 'operator': conversion from 'type1:field_bits' to 'type2:field_bits', possible loss of data
        /w14263 # 'classname': class has virtual functions, but destructor is not virtual instances of this class may not
                # be destructed correctly
        /w14265 # 'operator': member function does not override any base class virtual member function
        /w14287 # 'operator': unsigned/negative constant mismatch
        /we4289 # nonstandard extension used: 'variable': loop control variable declared in the for-loop is used outside
                # the for-loop scope
        /w14905 # wide string literal cast to 'LPSTR'
        /w14906 # string literal cast to 'LPWSTR'
        /w14928 # illegal copy-initialization; more than one user-defined conversion has been implicitly applied

        /wd4456 # declaration hides previous local declaration
        /wd4702 # unreachable code in third-party headers (e.g., range-v3)
        /wd4324 # structure padding due to alignment specifier
        /wd4996 # getenv deprecation warning (CRT_SECURE_NO_WARNINGS)
      )
      if(DWARFS_GIT_BUILD)
        target_compile_options(${tgt} PRIVATE /WX)
      endif()
    endif()
  endif()

  if(${tgt} MATCHES "fuzz_")
    target_compile_options(${tgt} PRIVATE
      -Wno-gnu-statement-expression-from-macro-expansion
      -Wno-old-style-cast
    )
  endif()

  set_property(TARGET ${tgt} PROPERTY CXX_STANDARD ${DWARFS_CXX_STANDARD})
  set_property(TARGET ${tgt} PROPERTY CXX_STANDARD_REQUIRED ON)
  set_property(TARGET ${tgt} PROPERTY CXX_EXTENSIONS OFF)

  if(ENABLE_COVERAGE)
    target_link_libraries(${tgt} PRIVATE dwarf_fnv1a.c.o)
  endif()

  if(DWARFS_FMT_LIB)
    target_link_libraries(${tgt} PRIVATE ${DWARFS_FMT_LIB})
  endif()

  if(WIN32)
    target_link_libraries(${tgt} PRIVATE ntdll.lib dbghelp.lib psapi.lib)
  endif()
endforeach()

# ============================================================================
# Coverage Configuration
# ============================================================================

if(ENABLE_COVERAGE)
  foreach(tgt ${BINARY_TARGETS} ${TEST_TARGETS})
    list(APPEND _PROFILE_OBJECTS -object=$<TARGET_FILE:${tgt}>)
  endforeach()
  # TODO: try `--unify-instantiations` instead of `-skip-branches` once we're on LLVM 21
  add_custom_target(
    coverage
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/coverage
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/coverage
    COMMAND llvm-profdata merge -sparse ${CMAKE_BINARY_DIR}/profile/*.profraw -o ${CMAKE_BINARY_DIR}/coverage/dwarfs.profdata
    COMMAND llvm-cov export -instr-profile=${CMAKE_BINARY_DIR}/coverage/dwarfs.profdata
                -skip-branches -ignore-filename-regex="\(fsst\|folly\|fbthrift\|build[^/]*\)/"
                -format=lcov ${_PROFILE_OBJECTS} > ${CMAKE_BINARY_DIR}/coverage/coverage.info
    COMMAND genhtml -o ${CMAKE_BINARY_DIR}/coverage/html
                --no-branch-coverage --missed --show-details
                --rc derive_function_end_line=1
                --ignore-errors inconsistent
                --ignore-errors corrupt
                ${CMAKE_BINARY_DIR}/coverage/coverage.info
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
  )
endif()

# ============================================================================
# Build-Type Specific Configuration
# ============================================================================

if(CMAKE_BUILD_TYPE STREQUAL Release)
  # not sure why exactly, copied from fsst/CMakeLists.txt
  set_source_files_properties(fsst/fsst_avx512.cpp PROPERTIES COMPILE_FLAGS -O1)
endif()

# ============================================================================
# Sanitizer Configuration
# ============================================================================

foreach(tgt dwarfs_test_helpers dwarfs_follybenchmark_lite thrift1
            ${LIBDWARFS_TARGETS} ${LIBDWARFS_OBJECT_TARGETS}
            ${BINARY_TARGETS} ${TEST_TARGETS} ${MAIN_TARGETS})
  if(TARGET ${tgt})
    # Check if target is INTERFACE library
    get_target_property(tgt_type ${tgt} TYPE)
    if(tgt_type STREQUAL "INTERFACE_LIBRARY")
      continue()
    endif()
    if(ENABLE_ASAN)
      target_compile_options(${tgt} PRIVATE -fsanitize=address
                                            -fno-omit-frame-pointer)
      target_link_options(${tgt} PRIVATE -fsanitize=address)
    endif()

    if(ENABLE_TSAN)
      target_compile_options(${tgt} PRIVATE -fsanitize=thread
                                            -fno-omit-frame-pointer)
      target_link_options(${tgt} PRIVATE -fsanitize=thread)
    endif()

    if(ENABLE_UBSAN)
      target_compile_options(${tgt} PRIVATE -fsanitize=undefined
                                            -fno-omit-frame-pointer)
      target_link_options(${tgt} PRIVATE -fsanitize=undefined)
    endif()

    if(ENABLE_SANITIZER)
      target_compile_options(${tgt} PRIVATE -fsanitize=${ENABLE_SANITIZER}
                                            -fno-omit-frame-pointer)
      target_link_options(${tgt} PRIVATE -fsanitize=${ENABLE_SANITIZER})
    endif()

  endif()
endforeach()

# ============================================================================
# Clean Targets
# ============================================================================

foreach(tgt ${TEST_TARGETS})
  list(APPEND REALCLEAN_FILES "${tgt}[1]_include.cmake")
endforeach()

foreach(tgt ${BINARY_TARGETS} ${TEST_TARGETS})
  list(APPEND REALCLEAN_FILES $<TARGET_FILE:${tgt}>.manifest)
  if(WIN32)
    list(APPEND REALCLEAN_FILES ${tgt}.ilk ${tgt}.pdb)
  endif()
endforeach()

add_custom_target(
  realclean
  COMMAND ${CMAKE_MAKE_PROGRAM} clean
  COMMAND ${CMAKE_COMMAND} -E rm -rf cmake_install.cmake install_manifest.txt
      dwarfs_install.cmake package_version.cmake
      CPackConfig.cmake CPackSourceConfig.cmake _CPack_Packages
      CTestTestfile.cmake Testing
      fbthrift folly zstd ricepp tools fake_folly
      include src thrift universal bin lib man1 man5
      vcpkg-manifest-install.log
      Makefile compile_commands.json
      artifacts.env source-artifacts.env
      default.profraw profile
      dwarfs-config-version.cmake
      dwarfs-config.cmake
      dwarfs.ilk dwarfs.pdb
      .ninja_deps .ninja_log build.ninja
      CMakeCache.txt
      ${REALCLEAN_FILES}
)

add_custom_target(
  distclean
  COMMAND ${CMAKE_MAKE_PROGRAM} realclean
  COMMAND ${CMAKE_COMMAND} -E rm -rf _deps
  COMMAND ${CMAKE_COMMAND} -E rm -rf CMakeFiles
)

# ============================================================================
# Code Formatting Targets
# ============================================================================

file(GLOB_RECURSE ALL_SOURCES LIST_DIRECTORIES false
        ${CMAKE_CURRENT_SOURCE_DIR}/ricepp/*.h
        ${CMAKE_CURRENT_SOURCE_DIR}/ricepp/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/*.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/tools/include/*.h
        ${CMAKE_CURRENT_SOURCE_DIR}/tools/src/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/sfx/stub.c
        ${CMAKE_CURRENT_SOURCE_DIR}/test/*.h
        ${CMAKE_CURRENT_SOURCE_DIR}/test/*.cpp)

add_custom_target(
  format
  COMMAND clang-format -i ${ALL_SOURCES})

add_custom_target(
  check-format
  COMMAND clang-format --dry-run --Werror ${ALL_SOURCES})

add_custom_target(
  tidy
  COMMAND run-clang-tidy -p ${CMAKE_BINARY_DIR} -quiet -use-color
              -source-filter='.*/src/.*'
              -header-filter='.*/include/dwarfs/.*'
  USES_TERMINAL)

add_custom_target(
  tidy-fix
  COMMAND run-clang-tidy -p ${CMAKE_BINARY_DIR} -quiet -use-color -fix
              -source-filter='.*/src/.*'
              -header-filter='.*/include/dwarfs/.*'
  USES_TERMINAL)

# ============================================================================
# Strip Targets (for Release Builds)
# ============================================================================

if(STATIC_BUILD_DO_NOT_USE OR APPLE)
  if(CMAKE_BUILD_TYPE MATCHES "Release|MinSizeRel")
    foreach(tgt ${BINARY_TARGETS})
      brand_elf_linux(${tgt})
      list(APPEND FILES_TO_STRIP $<TARGET_FILE:${tgt}>)
    endforeach()
    if(APPLE)
      add_custom_target(strip COMMAND strip ${FILES_TO_STRIP})
    else()
      set(STRIP_TOOL strip)
      if(DEFINED ENV{STRIP_TOOL})
        set(STRIP_TOOL $ENV{STRIP_TOOL})
      endif()
      add_custom_target(strip COMMAND ${STRIP_TOOL} $<IF:$<BOOL:${ENABLE_STACKTRACE}>,--strip-debug,--strip-all> ${FILES_TO_STRIP})
    endif()
  endif()
endif()