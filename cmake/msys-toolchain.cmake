# MSYS2 toolchain file for vcpkg
# This file tells CMake where to find the MSYS2/MinGW compiler
# Used via VCPKG_CHAINLOAD_TOOLCHAIN_FILE
#
# Note: GitHub Actions installs MinGW-w64 via MSYS2 at C:/mingw64
# The workflow adds /c/mingw64/bin to PATH, so gcc/g++ are found there

# Tell CMake we're cross-compiling to Windows
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_VERSION 10.0)

# IMPORTANT: Set the compiler directly, NOT using find_program
# vcpkg reads these variables to determine the compiler
# The workflow sets PATH so that gcc/g++ are found at C:/mingw64/bin
set(CMAKE_C_COMPILER "C:/mingw64/bin/gcc.exe" CACHE FILEPATH "C compiler")
set(CMAKE_CXX_COMPILER "C:/mingw64/bin/g++.exe" CACHE FILEPATH "C++ compiler")
set(CMAKE_AR "C:/mingw64/bin/ar.exe" CACHE FILEPATH "Archiver")
set(CMAKE_RANLIB "C:/mingw64/bin/ranlib.exe" CACHE FILEPATH "Ranlib")
set(CMAKE_STRIP "C:/mingw64/bin/strip.exe" CACHE FILEPATH "Strip")

# Tell CMake this is a MinGW environment (not MSYS)
set(MINGW TRUE)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
