# libarchive dependency configuration
# Used for archive extraction (tar, cpio, etc.)

if(DEFINED VCPKG_BUILD AND VCPKG_BUILD)
  # vcpkg mode: use FindLibArchive MODULE (not CONFIG)
  # vcpkg's libarchive uses CMake's FindLibArchive.cmake module
  find_package(LibArchive ${LIBARCHIVE_REQUIRED_VERSION} REQUIRED MODULE)

  # Create alias to match old pkg-config target name
  if(NOT TARGET PkgConfig::LIBARCHIVE)
    if(TARGET LibArchive::LibArchive)
      add_library(PkgConfig::LIBARCHIVE ALIAS LibArchive::LibArchive)
    endif()
  endif()

  message(STATUS "Using libarchive from vcpkg: ${LibArchive_VERSION}")
else()
  # Fallback to pkg-config (non-vcpkg builds)
  if(USE_SUBDIR_LIBARCHIVE)
    add_compile_definitions(LIBARCHIVE_STATIC)
    add_subdirectory(libarchive EXCLUDE_FROM_ALL)
    add_library(PkgConfig::LIBARCHIVE ALIAS archive_static)
    message(STATUS "Using libarchive from subdir")
  else()
    pkg_check_modules(LIBARCHIVE REQUIRED IMPORTED_TARGET libarchive>=${LIBARCHIVE_REQUIRED_VERSION})
    message(STATUS "Using libarchive from pkg-config: ${LIBARCHIVE_VERSION}")
  endif()
endif()