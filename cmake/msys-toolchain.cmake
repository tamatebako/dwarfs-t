# MSYS2 toolchain file for vcpkg
# This file tells CMake where to find the MSYS2/MinGW compiler
# Used via VCPKG_CHAINLOAD_TOOLCHAIN_FILE in CMakePresets.json
#
# Note: GitHub Actions installs MinGW-w64 via MSYS2 at C:/mingw64
# The workflow adds /c/mingw64/bin to PATH, so gcc/g++ are found there

# Tell CMake we're cross-compiling to MSYS
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_VERSION 10.0)

# MSYS2/MinGW compiler paths (from GitHub Actions installation)
set(CMAKE_C_COMPILER gcc)
set(CMAKE_CXX_COMPILER g++)

# Find the correct gcc from MinGW
find_program(CMAKE_C_COMPILER NAMES gcc x86_64-w64-mingw32-gcc HINTS "C:/mingw64/bin")
find_program(CMAKE_CXX_COMPILER NAMES g++ x86_64-w64-mingw32-g++ HINTS "C:/mingw64/bin")

# MinGW ar and ranlib
find_program(CMAKE_AR NAMES ar HINTS "C:/mingw64/bin")
find_program(CMAKE_RANLIB NAMES ranlib HINTS "C:/mingw64/bin")

# MinGW strip
find_program(CMAKE_STRIP NAMES strip HINTS "C:/mingw64/bin")

# Tell CMake this is a MSYS environment
set(MSYS 1)
