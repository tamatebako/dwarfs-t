# Custom MinGW toolchain override
# This file overrides CMake's default MinGW behavior to prevent
# the automatic addition of --out-implib which causes WinMain errors

# Disable the automatic import library generation for Windows-GNU
set(CMAKE_LINK_DEF_FILE_FLAG "-Wl,--output-def,")
set(CMAKE_LINK_IMPLIB_FLAG "")

# Override the platform file behavior
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS FALSE CACHE BOOL "" FORCE)

# For executables, don't generate import libraries
set(CMAKE_EXE_LINKER_FLAGS ${CMAKE_EXE_LINKER_FLAGS} "-Wl,--out-implib," CACHE STRING "" FORCE)

# Use static C++ runtime
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static-libgcc -static-libstdc++" CACHE STRING "" FORCE)
