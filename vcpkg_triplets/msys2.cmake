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

# ============================================================================
# CRITICAL: Enable FindBoost module for Boost detection
# ============================================================================
# CMake 3.30+ removed FindBoost module (available only if CMP0167 is not NEW)
# Setting CMP0167 to OLD enables FindBoost, which avoids the circular dependency:
#   - boost-assert configure needs BoostConfig.cmake
#   - But BoostConfig.cmake is only created after Boost is fully built
# FindBoost can find Boost components during configure without this circularity
if(POLICY CMP0167)
    cmake_policy(SET CMP0167 OLD)
    message(STATUS "MSYS toolchain: CMake policy CMP0167 set to OLD to enable FindBoost module")
endif()

# IMPORTANT: Do NOT set CMAKE_IMPORT_LIBRARY_SUFFIX
# Leaving this empty prevents CMake from adding --out-implib flag to executables
# The --out-implib flag causes MinGW to use GUI startup files expecting WinMain
# instead of console startup files expecting main()
set(CMAKE_IMPORT_LIBRARY_SUFFIX "" CACHE STRING "Disable import library generation for executables" FORCE)

# Ensure we find the MinGW compiler (in /c/mingw64/bin for MSYS2)
find_program(CMAKE_C_COMPILER gcc PATHS "C:/mingw64/bin" NO_DEFAULT_PATH)
find_program(CMAKE_CXX_COMPILER g++ PATHS "C:/mingw64/bin" NO_DEFAULT_PATH)
find_program(CMAKE_AR ar PATHS "C:/mingw64/bin" NO_DEFAULT_PATH)
find_program(CMAKE_RANLIB ranlib PATHS "C:/mingw64/bin" NO_DEFAULT_PATH)
find_program(CMAKE_STRIP strip PATHS "C:/mingw64/bin" NO_DEFAULT_PATH)

# ============================================================================
# MSYS Iconv Support
# ============================================================================
# vcpkg's libiconv port doesn't build for MSYS (it expects builtin iconv)
# However, FindIconv can't locate the builtin iconv without explicit paths
# We set paths to mingw-w64 libiconv which is compatible with vcpkg's compiler

# Set CMAKE_PREFIX_PATH to help FindIconv and other find modules
set(CMAKE_PREFIX_PATH "C:/msys64/mingw64" CACHE PATH "MinGW-w64 prefix for finding dependencies" FORCE)

# Explicitly set Iconv paths for FindIconv module
# These paths point to mingw-w64 libiconv installed by pacman
set(Iconv_INCLUDE_DIR "C:/msys64/mingw64/include" CACHE PATH "Iconv include directory" FORCE)
set(Iconv_LIBRARY "C:/msys64/mingw64/lib/libiconv.dll.a" CACHE FILEPATH "Iconv library file" FORCE)

# Also set as regular variables for compatibility
set(ICONV_INCLUDE_DIR "C:/msys64/mingw64/include" CACHE PATH "Iconv include directory" FORCE)
set(ICONV_LIBRARY "C:/msys64/mingw64/lib/libiconv.dll.a" CACHE FILEPATH "Iconv library file" FORCE)

message(STATUS "MSYS toolchain: Iconv paths set to mingw-w64 libiconv")
message(STATUS "  Iconv_INCLUDE_DIR: ${Iconv_INCLUDE_DIR}")
message(STATUS "  Iconv_LIBRARY: ${Iconv_LIBRARY}")
