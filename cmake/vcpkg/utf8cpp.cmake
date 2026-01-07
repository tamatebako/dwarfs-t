# utf8cpp dependency configuration
# Header-only library for UTF-8 string handling

if(DEFINED VCPKG_BUILD AND VCPKG_BUILD)
  # vcpkg mode: utf8cpp is header-only, use find_path
  # Note: utfcpp package installs headers in utf8cpp/ subdirectory

  # Try the global vcpkg install first, then fall back to the local vcpkg_installed in _deps
  find_path(UTF8CPP_INCLUDE_DIR
    NAMES utf8cpp/utf8.h
    PATHS
      /Users/mulgogi/src/external/vcpkg/installed/arm64-osx-static/include
      /Users/mulgogi/src/external/dwarfs/example/static-site-server/_deps/dwarfs-build/vcpkg_installed/arm64-osx-static/include
      /Users/mulgogi/src/external/dwarfs/_deps/dwarfs-build/vcpkg_installed/arm64-osx-static/include
    REQUIRED
  )

  if(NOT TARGET utf8cpp)
    add_library(utf8cpp INTERFACE IMPORTED)
    set_target_properties(utf8cpp PROPERTIES
      INTERFACE_INCLUDE_DIRECTORIES "${UTF8CPP_INCLUDE_DIR}"
    )
  endif()

  message(STATUS "Using utf8cpp from vcpkg: ${UTF8CPP_INCLUDE_DIR}")
else()
  # Non-vcpkg mode: try system or FetchContent
  find_path(UTF8CPP_INCLUDE_DIR
    NAMES utf8cpp/utf8.h
  )

  if(UTF8CPP_INCLUDE_DIR)
    if(NOT TARGET utf8cpp)
      add_library(utf8cpp INTERFACE IMPORTED)
      set_target_properties(utf8cpp PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${UTF8CPP_INCLUDE_DIR}"
      )
    endif()
    message(STATUS "Using system utf8cpp: ${UTF8CPP_INCLUDE_DIR}")
  else()
    # Fall back to FetchContent
    message(STATUS "utf8cpp not found, using FetchContent")
    include(FetchContent)

    FetchContent_Declare(
      utf8cpp
      GIT_REPOSITORY https://github.com/nemtrif/utfcpp.git
      GIT_TAG v4.0.6
    )

    FetchContent_MakeAvailable(utf8cpp)

    if(NOT TARGET utf8cpp)
      add_library(utf8cpp INTERFACE IMPORTED)
      set_target_properties(utf8cpp PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${utf8cpp_SOURCE_DIR}/include"
      )
    endif()
  endif()
endif()