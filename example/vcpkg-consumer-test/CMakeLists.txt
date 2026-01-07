cmake_minimum_required(VERSION 3.28.0)

project(dwarfs-header-installation-test)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

# Find DwarFS package (tests that it's installed correctly)
find_package(dwarfs REQUIRED CONFIG)

# Test executable that includes ALL critical headers
# This ensures the header installation is complete
add_executable(header_test header_test.cpp)
target_link_libraries(header_test 
  dwarfs::dwarfs_common
  dwarfs::dwarfs_reader 
  dwarfs::dwarfs_extractor
)

# Success message
add_custom_command(TARGET header_test POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E echo "✅ Header installation test PASSED - All critical headers found"
)