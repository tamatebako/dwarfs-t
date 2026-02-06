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

# IMPORTANT: Force console subsystem to prevent WinMain errors
# The --out-implib flag causes MinGW to use GUI startup files expecting WinMain
# instead of console startup files expecting main()
set(CMAKE_IMPORT_LIBRARY_SUFFIX "" CACHE STRING "Disable import library generation for executables" FORCE)
set(CMAKE_EXE_LINKER_FLAGS "-mconsole" CACHE STRING "Force console subsystem" FORCE)

# Override CMake's default rules to prevent --out-implib flag
# Set WIN32_EXECUTABLE to FALSE by default for all executables
set(CMAKE_WIN32_EXECUTABLE FALSE CACHE BOOL "Default to console executables" FORCE)

# CRITICAL: Override the link rule to remove --out-implib flag
# CMake's MinGW platform module adds --out-implib unconditionally
# which causes MinGW to use GUI startup files (crtexewin.o) expecting WinMain
# instead of console startup files (crt2.o) expecting main()
# We override CMAKE_CXX_LINK_EXECUTABLE to filter out --out-implib
set(CMAKE_CXX_LINK_EXECUTABLE
    "<CMAKE_CXX_COMPILER> <FLAGS> <CMAKE_CXX_LINK_FLAGS> <LINK_FLAGS> <OBJECTS> -o <TARGET> <LINK_LIBRARIES>"
    CACHE STRING "Link command without --out-implib" FORCE
)

# Ensure we find the MinGW compiler
find_program(CMAKE_C_COMPILER gcc)
find_program(CMAKE_CXX_COMPILER g++)
find_program(CMAKE_AR ar)
find_program(CMAKE_RANLIB ranlib)
find_program(CMAKE_STRIP strip)
