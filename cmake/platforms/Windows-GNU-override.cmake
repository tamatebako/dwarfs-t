# MinGW platform override
# This file overrides CMake's default Windows-GNU.cmake behavior
# to prevent automatic --out-implib addition

# Empty implib flag to prevent automatic generation
set(CMAKE_LINK_IMPLIB_FLAG "" CACHE STRING "Disable implib generation" FORCE)

# Disable GNU to MSVC import lib conversion
set(CMAKE_GNUtoMS 0 CACHE BOOL "Disable GNU to MSVC conversion" FORCE)

# Don't automatically add Windows flags
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS FALSE CACHE BOOL "Disable auto export" FORCE)
