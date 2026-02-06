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

# IMPORTANT: Disable --out-implib flag to prevent WinMain errors
# The --out-implib flag causes MinGW to use GUI startup files expecting WinMain
# instead of console startup files expecting main()
set(CMAKE_IMPORT_LIBRARY_SUFFIX "" CACHE STRING "Disable import library generation for executables" FORCE)

# Console mode is the default for MinGW - no -mconsole flag needed
# Just ensure WIN32_EXECUTABLE is FALSE (set in vcpkg triplet)
set(CMAKE_WIN32_EXECUTABLE FALSE CACHE BOOL "Default to console executables" FORCE)

# CRITICAL: Prevent MinGW from adding --out-implib to linker flags
# These variables control CMake's behavior for Windows executables
set(CMAKE_SUPPORT_WINDOWS_EXPORT_ALL_SYMBOLS FALSE CACHE BOOL "Disable automatic symbol exports" FORCE)
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS FALSE CACHE BOOL "Disable automatic symbol exports for executables" FORCE)

# Override the linker flags to prevent --out-implib
set(CMAKE_EXE_LINKER_FLAGS "" CACHE STRING "Empty linker flags to prevent --out-implib" FORCE)
set(CMAKE_CXX_LINK_FLAGS "" CACHE STRING "Empty C++ link flags to prevent --out-implib" FORCE)

# Override the link rules to prevent --out-implib
set(CMAKE_CXX_LINK_EXECUTABLE
    "<CMAKE_CXX_COMPILER> <FLAGS> <CMAKE_CXX_LINK_FLAGS> <LINK_FLAGS> <OBJECTS> -o <TARGET> <LINK_LIBRARIES>"
    CACHE STRING "Link command without --out-implib" FORCE
)
set(CMAKE_C_LINK_EXECUTABLE
    "<CMAKE_C_COMPILER> <FLAGS> <CMAKE_C_LINK_FLAGS> <LINK_FLAGS> <OBJECTS> -o <TARGET> <LINK_LIBRARIES>"
    CACHE STRING "Link command without --out-implib" FORCE
)

# Ensure we find the MinGW compiler
find_program(CMAKE_C_COMPILER gcc)
find_program(CMAKE_CXX_COMPILER g++)
find_program(CMAKE_AR ar)
find_program(CMAKE_RANLIB ranlib)
find_program(CMAKE_STRIP strip)
