# Custom MinGW toolchain file for vcpkg
# This tells vcpkg to use MinGW GCC for building dependencies on Windows

set(CMAKE_C_COMPILER gcc)
set(CMAKE_CXX_COMPILER g++)
set(CMAKE_AR ar)
set(CMAKE_RANLIB ranlib)
set(CMAKE_STRIP strip)

# Tell CMake this is a MinGW environment
set(MINGW TRUE)
set(CMAKE_SYSTEM_NAME Windows)

# IMPORTANT: Do NOT set CMAKE_IMPORT_LIBRARY_SUFFIX
# Leaving this empty prevents CMake from adding --out-implib flag to executables
# The --out-implib flag causes MinGW to use GUI startup files expecting WinMain
# instead of console startup files expecting main()
set(CMAKE_IMPORT_LIBRARY_SUFFIX "" CACHE STRING "Disable import library generation for executables" FORCE)

# Ensure we find the MinGW compiler
find_program(CMAKE_C_COMPILER gcc)
find_program(CMAKE_CXX_COMPILER g++)
find_program(CMAKE_AR ar)
find_program(CMAKE_RANLIB ranlib)
find_program(CMAKE_STRIP strip)
