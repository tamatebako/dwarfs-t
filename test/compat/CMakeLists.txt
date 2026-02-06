# CMakeLists.txt for Homebrew Compatibility Tests
# vim:set ts=2 sw=2 sts=2 et:

cmake_minimum_required(VERSION 3.20)

# Determine if we're building as part of the main project or standalone
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(dwarfs-compat-tests LANGUAGES CXX)

  # Standard setup for standalone build
  set(CMAKE_CXX_STANDARD 20)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)

  # Find dependencies
  find_package(OpenSSL REQUIRED)

  # These would be found when building as part of dwarfs
  # For standalone builds, we need to locate them
  if(NOT TARGET dwarfs::metadata)
    message(FATAL_ERROR "dwarfs library not found. Build from main project.")
  endif()

  if(NOT TARGET GTest::gtest_main)
    find_package(GTest REQUIRED)
  endif()
endif()

# Include directories for the compat library
set(COMPAT_LIB_INCLUDE_DIRS
  ${CMAKE_CURRENT_SOURCE_DIR}/lib
  ${CMAKE_SOURCE_DIR}/include
)

# Source files for the compat library
set(COMPAT_LIB_SOURCES
  lib/fixture_cache.cpp
)

# Unix-specific source files (not compatible with Windows)
if(NOT WIN32)
  list(APPEND COMPAT_LIB_SOURCES
    lib/homebrew_detector.cpp
    lib/fixture_generator.cpp
    lib/compatibility_tester.cpp
  )
endif()

# Header files for the compat library
set(COMPAT_LIB_HEADERS
  lib/fixture_cache.h
)

# Unix-specific header files (not compatible with Windows)
if(NOT WIN32)
  list(APPEND COMPAT_LIB_HEADERS
    lib/homebrew_detector.h
    lib/fixture_generator.h
    lib/compatibility_tester.h
  )
endif()

# Create the compat library
add_library(dwarfs-compat-lib STATIC ${COMPAT_LIB_SOURCES})
target_include_directories(dwarfs-compat-lib
  PUBLIC
    ${COMPAT_LIB_INCLUDE_DIRS}
)

# Link required libraries
target_link_libraries(dwarfs-compat-lib
  PUBLIC
    OpenSSL::Crypto
)

# Add alias for consistency with other dwarfs libraries
add_library(dwarfs::compat-lib ALIAS dwarfs-compat-lib)

# Test executables - only build on Unix platforms
if(NOT WIN32)
  set(TEST_TARGETS
    read_homebrew_files_test
    write_compatible_files_test
    round_trip_test
  )
endif()

foreach(test_target ${TEST_TARGETS})
  # Source file for this test
  set(test_source "tests/${test_target}.cpp")

  # Create test executable
  add_executable(${test_target} ${test_source})

  # Link with dependencies
  target_link_libraries(${test_target}
    PRIVATE
      dwarfs-compat-lib
      GTest::gtest_main
      GTest::gtest
      GTest::gmock
      GTest::gmock_main
  )

  # Add to test suite
  add_test(NAME ${test_target} COMMAND ${test_target})

  # Group tests in IDE
  set_target_properties(${test_target} PROPERTIES
    FOLDER "Tests/Compat"
  )
endforeach()

# Custom targets for workflow

# Target to check Homebrew installation
add_custom_target(check-homebrew
  COMMAND ${BASH_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/check_homebrew.sh
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMENT "Checking Homebrew dwarfs installation..."
  VERBATIM
)

# Target to generate fixtures
add_custom_target(generate-fixtures
  COMMAND ${BASH_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/generate_fixtures.sh
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMENT "Generating Homebrew dwarfs test fixtures..."
  VERBATIM
)

# Target to validate fixtures
add_custom_target(validate-fixtures
  COMMAND ${BASH_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/validate_fixtures.sh
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMENT "Validating Homebrew dwarfs test fixtures..."
  VERBATIM
)

# Target to run all compat tests
add_custom_target(test-compat
  COMMAND ${BASH_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/run_tests.sh
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMENT "Running Homebrew compatibility tests..."
  VERBATIM
)

# Target to run all compat tests (CTest)
add_custom_target(test-compat-ctest
  COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -L compat
  DEPENDS ${TEST_TARGETS}
  COMMENT "Running compat tests via CTest..."
  VERBATIM
)

# Installation rules
install(TARGETS dwarfs-compat-lib
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
)

install(FILES ${COMPAT_LIB_HEADERS}
  DESTINATION include/dwarfs/test/compat/lib
)

install(DIRECTORY scripts/
  DESTINATION lib/dwarfs/test/compat/scripts
  FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                    GROUP_READ GROUP_EXECUTE
                    WORLD_READ WORLD_EXECUTE
  USE_SOURCE_PERMISSIONS
)

# Configuration
option(DWARFS_COMPAT_TESTS_ENABLE "Enable Homebrew compatibility tests" ON)
option(DWARFS_COMPAT_TESTS_AUTO_RUN "Automatically run compat tests with 'make test'" ${DWARFS_COMPAT_TESTS_ENABLE})

# Set property for CTest labels
set_tests_properties(${TEST_TARGETS} PROPERTIES LABELS "compat")

# Disable auto-run if requested
if(NOT DWARFS_COMPAT_TESTS_AUTO_RUN)
  set_tests_properties(${TEST_TARGETS} PROPERTIES DISABLED TRUE)
endif()

# Summary message
message(STATUS "")
message(STATUS "Homebrew Compatibility Tests Configuration:")
message(STATUS "  Enabled:           ${DWARFS_COMPAT_TESTS_ENABLE}")
message(STATUS "  Auto-run with test: ${DWARFS_COMPAT_TESTS_AUTO_RUN}")
message(STATUS "  Fixtures dir:      ${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
message(STATUS "")
message(STATUS "Available targets:")
message(STATUS "  check-homebrew    - Check Homebrew dwarfs installation")
message(STATUS "  generate-fixtures - Generate test fixtures")
message(STATUS "  validate-fixtures - Validate test fixtures")
message(STATUS "  test-compat       - Run all compat tests via script")
message(STATUS "  test-compat-ctest - Run all compat tests via CTest")
message(STATUS "")
