# Custom MSYS2 toolchain file for vcpkg
# This tells vcpkg to use MSYS2 GCC for building dependencies on Windows

set(CMAKE_C_COMPILER gcc)
set(CMAKE_CXX_COMPILER g++)
set(CMAKE_AR ar)
set(CMAKE_RANLIB ranlib)
set(CMAKE_STRIP strip)

# Tell CMake this is a MinGW environment (MSYS2 uses MinGW)
set(MINGW TRUE)
set(CMAKE_SYSTEM_NAME Windows)

# Ensure we find the MinGW compiler (in /c/mingw64/bin for MSYS2)
find_program(CMAKE_C_COMPILER gcc PATHS "C:/mingw64/bin" NO_DEFAULT_PATH)
find_program(CMAKE_CXX_COMPILER g++ PATHS "C:/mingw64/bin" NO_DEFAULT_PATH)
find_program(CMAKE_AR ar PATHS "C:/mingw64/bin" NO_DEFAULT_PATH)
find_program(CMAKE_RANLIB ranlib PATHS "C:/mingw64/bin" NO_DEFAULT_PATH)
find_program(CMAKE_STRIP strip PATHS "C:/mingw64/bin" NO_DEFAULT_PATH)
