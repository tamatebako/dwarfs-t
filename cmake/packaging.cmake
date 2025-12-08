#
# Copyright (c) Marcus Holland-Moritz
#
# This file is part of dwarfs.
#
# dwarfs is free software: you can redistribute it and/or modify it under the
# terms of the GNU General Public License as published by the Free Software
# Foundation, either version 3 of the License, or (at your option) any later
# version.
#
# dwarfs is distributed in the hope that it will be useful, but WITHOUT ANY
# WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
# A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along with
# dwarfs.  If not, see <https://www.gnu.org/licenses/>.
#

# Conditional minimum version for tebako compatibility
if(DEFINED ENV{TEBAKO_BUILD} OR TEBAKO_BUILD)
  cmake_minimum_required(VERSION 3.24.0)
else()
  cmake_minimum_required(VERSION 3.28.0)
endif()

# ============================================================================
# Artifact ID Generation
# ============================================================================

set(DWARFS_ARTIFACT_ID "${PRJ_VERSION_FULL}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  set(DWARFS_ARTIFACT_ID "${DWARFS_ARTIFACT_ID}-gcc")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  set(DWARFS_ARTIFACT_ID "${DWARFS_ARTIFACT_ID}-clang")
endif()
if(DWARFS_OPTIMIZE)
  set(DWARFS_ARTIFACT_ID "${DWARFS_ARTIFACT_ID}-O${DWARFS_OPTIMIZE}")
endif()
if(CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
  set(DWARFS_ARTIFACT_ID "${DWARFS_ARTIFACT_ID}-reldbg")
elseif(CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
  set(DWARFS_ARTIFACT_ID "${DWARFS_ARTIFACT_ID}-minsize")
elseif(CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(DWARFS_ARTIFACT_ID "${DWARFS_ARTIFACT_ID}-debug")
endif()
if(ENABLE_STACKTRACE)
  set(DWARFS_ARTIFACT_ID "${DWARFS_ARTIFACT_ID}-stacktrace")
endif()
if(DWARFS_ARTIFACT_SUFFIX)
  set(DWARFS_ARTIFACT_ID "${DWARFS_ARTIFACT_ID}${DWARFS_ARTIFACT_SUFFIX}")
endif()
if(CMAKE_CROSSCOMPILING)
  set(DWARFS_ARTIFACT_ID "${DWARFS_ARTIFACT_ID}-cross-${CMAKE_HOST_SYSTEM_PROCESSOR}")
endif()

# ============================================================================
# Universal Binary Creation
# ============================================================================

if(STATIC_BUILD_DO_NOT_USE OR WIN32)
  if(TARGET dwarfsuniversal)
    list(APPEND UNIVERSAL_TARGETS dwarfsuniversal)
  endif()

  if(TARGET dwarfsfuseextract)
    list(APPEND UNIVERSAL_TARGETS dwarfsfuseextract)
  endif()

  # ppc64le/ppc64 are theoretically supported by UPX, but they currently crash
  #### skip for everything but Windows and use our own sfx stub
  #### we can always prepare a upx-compressed version for release
  # if(${CMAKE_SYSTEM_PROCESSOR} MATCHES "^(i386|x86_64|AMD64|aarch64|arm)$")
  if(WIN32)
    find_program(UPX_EXE upx upx.exe PATHS "c:/bin" DOC "ultimate packer for executables")
  endif()

  foreach(tgt ${UNIVERSAL_TARGETS})
    get_target_property(TARGET_FILENAME ${tgt} OUTPUT_NAME)
    get_filename_component(TARGET_FILENAME ${TARGET_FILENAME} NAME_WLE)
    set(UNIVERSAL_OUT ${TARGET_FILENAME}-${DWARFS_ARTIFACT_ID}${CMAKE_EXECUTABLE_SUFFIX})
    message(STATUS "Creating universal binary for ${tgt} -> ${UNIVERSAL_OUT}")

    if(ENABLE_STACKTRACE)
      message(WARNING "UPX compression is disabled with ENABLE_STACKTRACE")

      add_custom_command(
        OUTPUT ${UNIVERSAL_OUT}
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${tgt}> ${UNIVERSAL_OUT}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      )
    else()
      if(UPX_EXE)
        # upx -9 is a good compromise between compression ratio and speed
        # also, anything above --best increases the startup time of the compressed
        # executable significantly
        add_custom_command(
          OUTPUT ${UNIVERSAL_OUT}
          COMMAND ${UPX_EXE} -9 --best -o ${UNIVERSAL_OUT} $<TARGET_FILE:${tgt}>
          WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        )
      elseif(TARGET sfxstub_lz4 OR TARGET sfxstub_zstd_minimal)
        # Fallback if UPX is not found or doesn't support the target architecture.
        # Prefer lz4 (as it is faster), otherwise use zstd.
        if(TARGET sfxstub_zstd)
          set(_sfx_stub_target sfxstub_zstd)
          # Sweet spot between compression ratio and decompression speed.
          # set(_sfx_options "--level=16")
          # We'll use the default (`--level=19`, i.e. maximum compression)
          # for now and will potentially re-pack for release.
        else()
          set(_sfx_stub_target sfxstub_lz4)
          set(_sfx_options "--lz4")
        endif()
        add_custom_command(
          OUTPUT ${UNIVERSAL_OUT}
          COMMAND ${CMAKE_SOURCE_DIR}/sfx/pack.py
                      ${_sfx_options}
                      --stub $<TARGET_FILE:${_sfx_stub_target}>
                      --input $<TARGET_FILE:${tgt}>
                      --output ${UNIVERSAL_OUT}
          COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} ${CMAKE_CURRENT_BINARY_DIR}/${UNIVERSAL_OUT}
                      --extract-wrapped-binary ${UNIVERSAL_OUT}.extracted
          COMMAND ${CMAKE_COMMAND} -E compare_files ${UNIVERSAL_OUT}.extracted $<TARGET_FILE:${tgt}>
          COMMAND ${CMAKE_COMMAND} -E remove ${UNIVERSAL_OUT}.extracted
          WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
          COMMENT "UPX not found/supported, using ${_sfx_stub_target} for universal binary"
        )
      else()
        add_custom_command(
          OUTPUT ${UNIVERSAL_OUT}
          COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${tgt}> ${UNIVERSAL_OUT}
          WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
          COMMENT "UPX not found/supported, copying binary without compression"
        )
      endif()

      list(APPEND UNIVERSAL_UPX_TARGETS ${CMAKE_CURRENT_BINARY_DIR}/${UNIVERSAL_OUT})
      set(UNIVERSAL_ENV_LIST "${UNIVERSAL_ENV_LIST}${tgt}_binary=${UNIVERSAL_OUT}\n")
    endif()

    if(WITH_FUSE_DRIVER)
      # install sphinx
      set(UNIVERSAL_FUSE_DRIVER_HELP_INSTALL "build_articles/utils/fuse_driver_help/html")
      install(FILES ${UNIVERSAL_FUSE_DRIVER_HELP_INSTALL}/_sources/index.txt
	      DESTINATION doc/_sources)
      install(FILES ${UNIVERSAL_FUSE_DRIVER_HELP_INSTALL}/objects.inv
	      DESTINATION doc)

      # install fuse-cheatsheet.txt to man-formatted fuse-driver-help.1
      set(FUSE_DRIVER_HELP_MAN_ART "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_DATAROOTDIR}/doc/fuse-driver-help.1")
      install_file_trick_sources(doc/_downloads/fuse-cheatsheet.txt
                                   DocToManPatch.txt
                                   input sed "/^\\.SH SYNOPSIS/D; /^\\.SH DESCRIPTION D/"g
                                   )
      add_custom_command(OUTPUT ${FUSE_DRIVER_HELP_MAN_ART}
                         COMMAND sed ${DocToManPatch} ${FuseToMan} ${CMAKE_CURRENT_SOURCE_DIR}/${UNIVERSAL_FUSE_DRIVER_HELP_INSTALL}/index.html > ${FUSE_DRIVER_HELP_MAN_ART}
                         DEPENDS ${UNIVERSAL_FUSE_DRIVER_HELP})
      list(APPEND REALCLEAN_FILES ${FUSE_DRIVER_HELP_MAN_ART})
    endif()
  endforeach()

  add_custom_target(universal_upx DEPENDS ${UNIVERSAL_UPX_TARGETS})
endif()

# ============================================================================
# GitHub Artifacts Configuration
# ============================================================================

if(DEFINED ENV{GITHUB_REF_TYPE})
  message(STATUS "GITHUB_REF_TYPE: $ENV{GITHUB_REF_TYPE}")
  message(STATUS "GITHUB_REF_NAME: $ENV{GITHUB_REF_NAME}")
  message(STATUS "GITHUB_RUN_ID: $ENV{GITHUB_RUN_ID}")
  message(STATUS "GITHUB_RUN_NUMBER: $ENV{GITHUB_RUN_NUMBER}")
  message(STATUS "GITHUB_RUN_ATTEMPT: $ENV{GITHUB_RUN_ATTEMPT}")

  if("$ENV{GITHUB_REF_TYPE}" STREQUAL "tag")
    set(ARTIFACTS_SUBDIR "releases/$ENV{GITHUB_REF_NAME}@${PRJ_GIT_REV}")
  else()
    set(ARTIFACTS_SUBDIR "builds/$ENV{GITHUB_RUN_NUMBER}.$ENV{GITHUB_RUN_ATTEMPT}-${PRJ_VERSION_FULL}")
  endif()

  set(ARTIFACTS_FULL_PATH "${DWARFS_ARTIFACTS_DIR}/${ARTIFACTS_SUBDIR}")

  if(WIN32)
    set(PACKAGE_EXT ".7z")
  else()
    set(PACKAGE_EXT ".tar.zst")
  endif()

  set(SOURCE_TARBALL "${CMAKE_PROJECT_NAME}-${PRJ_VERSION_FULL}${PACKAGE_EXT}")

  add_custom_target(copy_source_artifacts
    COMMAND ${CMAKE_COMMAND} -E make_directory ${ARTIFACTS_FULL_PATH}
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/${SOURCE_TARBALL} ${ARTIFACTS_FULL_PATH}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${DWARFS_ARTIFACTS_DIR}/cache
    COMMAND ${CMAKE_COMMAND} -E create_symlink ../${ARTIFACTS_SUBDIR}/${SOURCE_TARBALL}
                ${DWARFS_ARTIFACTS_DIR}/cache/dwarfs-source-$ENV{GITHUB_RUN_NUMBER}${PACKAGE_EXT}
  )

  if(WIN32)
    set_source_files_properties(_copy_artifacts PROPERTIES SYMBOLIC ON)
  endif()
endif()

# ============================================================================
# CPack Configuration
# ============================================================================

configure_file("${PROJECT_SOURCE_DIR}/cmake/dwarfs_install.cmake.in" dwarfs_install.cmake @ONLY)
set(CPACK_INSTALL_SCRIPT "${CMAKE_CURRENT_BINARY_DIR}/dwarfs_install.cmake")

if(WIN32)
  # set(CPACK_GENERATOR "NSIS;ZIP;7Z")
  set(CPACK_GENERATOR "7Z")
else()
  # use TZST and later re-pack as TXZ
  set(CPACK_GENERATOR "TZST")
endif()
set(CPACK_SOURCE_GENERATOR "${CPACK_GENERATOR}")
set(CPACK_THREADS 0)
set(CPACK_PACKAGE_VERSION_MAJOR "${PRJ_VERSION_MAJOR}")
set(CPACK_PACKAGE_VERSION_MINOR "${PRJ_VERSION_MINOR}")
set(CPACK_PACKAGE_VERSION_PATCH "${PRJ_VERSION_PATCH}")
set(CPACK_SOURCE_PACKAGE_FILE_NAME "${CMAKE_PROJECT_NAME}-${PRJ_VERSION_FULL}")
set(CPACK_PACKAGE_FILE_NAME "${CMAKE_PROJECT_NAME}-${DWARFS_ARTIFACT_ID}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "dwarfs - A high compression read-only file system")
set(CPACK_PACKAGE_VENDOR "Marcus Holland-Moritz <github@mhxnet.de>")
set(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
list(APPEND CPACK_SOURCE_IGNORE_FILES
  "\\.git/"
  "${CMAKE_SOURCE_DIR}/build.*"
  "${CMAKE_SOURCE_DIR}/@"
  "/\\."
  ".*~$"
  "${CMAKE_SOURCE_DIR}/doc/.*\\.png$"
  "${CMAKE_SOURCE_DIR}/doc/.*\\.gif$"
)


set(CPACK_VERBATIM_VARIABLES YES)

include(CPack)