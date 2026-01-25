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

# ============================================================================
# Tool Targets (mkdwarfs, dwarfsck, dwarfsextract)
# ============================================================================

if(WITH_TOOLS)
  foreach(tgt mkdwarfs dwarfsck dwarfsextract)
    # Create empty secondary libraries for Thrift-specific functionality
    if(DWARFS_HAVE_EXPERIMENTAL_THRIFT)
      add_library(${tgt}_secondary INTERFACE)
    endif()

    add_library(${tgt}_main OBJECT tools/src/${tgt}_main.cpp)
    target_link_libraries(${tgt}_main PRIVATE dwarfs_tool dwarfs_tool_support)
    if(DWARFS_HAVE_EXPERIMENTAL_THRIFT)
      target_link_libraries(${tgt}_main PRIVATE ${tgt}_secondary)
    endif()
    add_executable(${tgt} tools/src/${tgt}.cpp)
    target_link_libraries(${tgt} PRIVATE ${tgt}_main)
    target_link_libraries(${tgt} PRIVATE dwarfs_tool)
    if(TARGET PkgConfig::FUSE_T)
      target_link_libraries(${tgt}_main PRIVATE PkgConfig::FUSE_T)
    elseif(APPLE AND FUSE_FOUND)
      target_link_libraries(${tgt}_main PRIVATE PkgConfig::FUSE)
    elseif(FUSE3_FOUND)
      target_link_libraries(${tgt}_main PRIVATE PkgConfig::FUSE3)
    endif()

    list(APPEND MAIN_TARGETS ${tgt}_main)
    list(APPEND BINARY_TARGETS ${tgt})

    install(TARGETS ${tgt} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    if(MSVC)
      install(FILES $<$<CXX_COMPILER_ID:MSVC>:$<TARGET_PDB_FILE:${tgt}>> DESTINATION ${CMAKE_INSTALL_BINDIR} OPTIONAL)
    endif()
    if(NOT WIN32)
      install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/doc/completions/bash/${tgt}
              DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/${BASH_INSTALL_PATH})
      install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/doc/completions/zsh/_${tgt}
              DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/${ZSH_INSTALL_PATH})
    endif()
  endforeach()

  # NOTE: mkdwarfs-specific sources (options_parser, handlers) are now in dwarfs_tool_support
  # No need to add them here - they're compiled once in the library

  target_link_libraries(mkdwarfs_main PRIVATE dwarfs_writer dwarfs_reader)
  target_link_libraries(dwarfsck_main PRIVATE dwarfs_reader)
  target_link_libraries(dwarfsextract_main PRIVATE dwarfs_extractor)

  # Executables also need the same libraries for final linking
  target_link_libraries(mkdwarfs PRIVATE dwarfs_writer dwarfs_reader dwarfs_tool_support)
  target_link_libraries(dwarfsck PRIVATE dwarfs_reader dwarfs_tool_support)
  target_link_libraries(dwarfsextract PRIVATE dwarfs_extractor dwarfs_tool_support)

  # Link jemalloc to executables (required by Folly in libraries)
  if(USE_JEMALLOC)
    if(TARGET jemalloc::jemalloc)
      target_link_libraries(mkdwarfs PRIVATE jemalloc::jemalloc)
      target_link_libraries(dwarfsck PRIVATE jemalloc::jemalloc)
      target_link_libraries(dwarfsextract PRIVATE jemalloc::jemalloc)
    elseif(TARGET PkgConfig::JEMALLOC)
      target_link_libraries(mkdwarfs PRIVATE PkgConfig::JEMALLOC)
      target_link_libraries(dwarfsck PRIVATE PkgConfig::JEMALLOC)
      target_link_libraries(dwarfsextract PRIVATE PkgConfig::JEMALLOC)
    endif()
  endif()

  if(DWARFS_HAVE_EXPERIMENTAL_THRIFT)
    target_link_libraries(mkdwarfs_main PRIVATE dwarfs_rewrite)
    target_link_libraries(mkdwarfs PRIVATE dwarfs_rewrite)
  endif()

  if(WITH_UNIVERSAL_BINARY)
    add_executable(dwarfsuniversal tools/src/universal.cpp)
    list(APPEND BINARY_TARGETS dwarfsuniversal)

    target_compile_definitions(dwarfsuniversal PRIVATE
            DWARFS_UNIVERSAL_NAME=\"dwarfs-universal\"
            DWARFS_UNIVERSAL_MKDWARFS
            DWARFS_UNIVERSAL_DWARFSCK
            DWARFS_UNIVERSAL_DWARFSEXTRACT
    )
    target_link_libraries(dwarfsuniversal PRIVATE
            mkdwarfs_main dwarfsck_main dwarfsextract_main
    )
    set_target_properties(dwarfsuniversal PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY universal
            OUTPUT_NAME dwarfs-universal
    )
    target_link_libraries(dwarfs_main PRIVATE dwarfs_reader)
    target_link_libraries(dwarfs_main PRIVATE dwarfs_tool)
  endif()

  if(WITH_FUSE_EXTRACT_BINARY)
    add_executable(dwarfsfuseextract tools/src/universal.cpp)
    list(APPEND BINARY_TARGETS dwarfsfuseextract)

    target_compile_definitions(dwarfsfuseextract PRIVATE
            DWARFS_UNIVERSAL_NAME=\"dwarfs-fuse-extract\"
            DWARFS_UNIVERSAL_DWARFSEXTRACT
    )
    target_link_libraries(dwarfsfuseextract PRIVATE
            dwarfsextract_main
    )
    set_target_properties(dwarfsfuseextract PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY universal
            OUTPUT_NAME dwarfs-fuse-extract
    )
    target_link_libraries(dwarfs_main PRIVATE dwarfs_reader)
    target_link_libraries(dwarfs_main PRIVATE dwarfs_tool)
  endif()
endif()

# ============================================================================
# Additional Tool Binaries (pxattr, example)
# ============================================================================

if(WITH_PXATTR)
  add_executable(pxattr tools/src/pxattr.cpp)
  list(APPEND BINARY_TARGETS pxattr)
  install(TARGETS pxattr RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
  if(MSVC)
    install(FILES $<$<CXX_COMPILER_ID:MSVC>:$<TARGET_PDB_FILE:pxattr>> DESTINATION ${CMAKE_INSTALL_BINDIR} OPTIONAL)
  endif()
endif()

if(WITH_EXAMPLE)
  add_executable(example example/example.cpp)
  target_link_libraries(example PRIVATE dwarfs_reader dwarfs_extractor)
  list(APPEND BINARY_TARGETS example)
endif()

# ============================================================================
# FUSE Driver (dwarfs, dwarfs2)
# ============================================================================

if(WITH_FUSE_DRIVER)
  include(${CMAKE_SOURCE_DIR}/cmake/need_fuse.cmake)

  # Configure FUSE definitions for dwarfs_reader library (contains fuse_driver.cpp)
  if(TARGET dwarfs_reader)
    target_compile_definitions(dwarfs_reader PRIVATE _FILE_OFFSET_BITS=64)
    if(FUSE_IMPLEMENTATION STREQUAL "fuse-t")
      target_compile_definitions(dwarfs_reader PRIVATE FUSE_USE_VERSION=31 DWARFS_USE_FUSE_T)
      # FUSE-T include directories are provided by PkgConfig::FUSE_T target
      # Don't use hard-coded paths - let pkg-config provide the correct paths
    elseif(FUSE3_FOUND)
      target_compile_definitions(dwarfs_reader PRIVATE FUSE_USE_VERSION=35)
    elseif(FUSE_FOUND)
      target_compile_definitions(dwarfs_reader PRIVATE FUSE_USE_VERSION=29)
    elseif(WIN32 AND WINFSP)
      target_compile_definitions(dwarfs_reader PRIVATE FUSE_USE_VERSION=32 DWARFS_FUSE_LOWLEVEL=0)
    endif()

    # Link FUSE library to dwarfs_reader
    if(TARGET PkgConfig::FUSE_T)
      target_link_libraries(dwarfs_reader PRIVATE PkgConfig::FUSE_T)
    elseif(TARGET PkgConfig::FUSE3)
      target_link_libraries(dwarfs_reader PRIVATE PkgConfig::FUSE3)
    elseif(TARGET PkgConfig::FUSE)
      target_link_libraries(dwarfs_reader PRIVATE PkgConfig::FUSE)
    elseif(WINFSP)
      target_link_libraries(dwarfs_reader PRIVATE ${WINFSP})
    endif()
  endif()

  file(RELATIVE_PATH relative_sbindir_to_bindir "${CMAKE_INSTALL_FULL_SBINDIR}" "${CMAKE_INSTALL_FULL_BINDIR}")

  if(FUSE3_FOUND OR WINFSP OR APPLE)
    add_library(dwarfs_main OBJECT tools/src/dwarfs_main.cpp)
    target_compile_definitions(dwarfs_main PRIVATE _FILE_OFFSET_BITS=64)
    if(APPLE AND FUSE_IMPLEMENTATION STREQUAL "fuse-t")
      target_compile_definitions(dwarfs_main PRIVATE FUSE_USE_VERSION=31 DWARFS_USE_FUSE_T)
      # FUSE-T include directories are provided by PkgConfig::FUSE_T target
      # Don't use hard-coded paths - let pkg-config provide the correct paths
    elseif(FUSE3_FOUND)
      target_compile_definitions(dwarfs_main PRIVATE FUSE_USE_VERSION=35)
    endif()
    target_link_libraries(dwarfs_main PRIVATE dwarfs_tool dwarfs_reader dwarfs_tool_support)
    if(TARGET PkgConfig::FUSE_T)
      target_link_libraries(dwarfs_main PRIVATE PkgConfig::FUSE_T)
    elseif(APPLE AND FUSE_FOUND)
      target_link_libraries(dwarfs_main PRIVATE PkgConfig::FUSE)
    elseif(FUSE3_FOUND)
      target_link_libraries(dwarfs_main PRIVATE PkgConfig::FUSE3)
    endif()
    add_executable(dwarfs-bin tools/src/dwarfs.cpp)
    target_link_libraries(dwarfs-bin PRIVATE dwarfs_main)
    target_link_libraries(dwarfs-bin PRIVATE dwarfs_tool)
    set_target_properties(dwarfs-bin PROPERTIES OUTPUT_NAME dwarfs)
    if(WINFSP)
      target_compile_definitions(dwarfs_main PRIVATE FUSE_USE_VERSION=32
                                                     DWARFS_FUSE_LOWLEVEL=0)
      target_include_directories(dwarfs_main SYSTEM PRIVATE "${WINFSP_PATH}/inc")
      target_link_libraries(dwarfs_main PRIVATE ${WINFSP})
      target_link_libraries(dwarfs-bin PRIVATE delayimp.lib)
      target_link_options(dwarfs-bin PRIVATE /DELAYLOAD:winfsp-x64.dll)
      if(TARGET dwarfsuniversal)
        target_link_libraries(dwarfsuniversal PRIVATE delayimp.lib)
        target_link_options(dwarfsuniversal PRIVATE /DELAYLOAD:winfsp-x64.dll)
        target_compile_definitions(dwarfsuniversal PRIVATE DWARFS_UNIVERSAL_FUSE_DRIVER)
      endif()
      if(TARGET dwarfsfuseextract)
        target_link_libraries(dwarfsfuseextract PRIVATE delayimp.lib)
        target_link_options(dwarfsextract PRIVATE /DELAYLOAD:winfsp-x64.dll)
        target_compile_definitions(dwarfsfuseextract PRIVATE DWARFS_UNIVERSAL_FUSE_DRIVER)
      endif()
      if(WINFSP)
        install(TARGETS dwarfs-bin RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
        if(MSVC)
          install(FILES $<$<CXX_COMPILER_ID:MSVC>:$<TARGET_PDB_FILE:dwarfs-bin>> DESTINATION ${CMAKE_INSTALL_BINDIR} OPTIONAL)
        endif()
      else()
        install(TARGETS dwarfs-bin RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
        install(CODE "\
          file(MAKE_DIRECTORY \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_SBINDIR}\")
          file(CREATE_LINK \"${relative_sbindir_to_bindir}/dwarfs\"
                           \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_SBINDIR}/mount.dwarfs\"
                           SYMBOLIC)")
        install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/doc/completions/bash/dwarfs
                DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/${BASH_INSTALL_PATH})
        install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/doc/completions/zsh/_dwarfs
                DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/${ZSH_INSTALL_PATH})
      endif()
      list(APPEND BINARY_TARGETS dwarfs-bin)
      list(APPEND MAIN_TARGETS dwarfs_main)
      target_link_libraries(dwarfs_main PRIVATE dwarfs_reader
                           PRIVATE dwarfs_tool
                           PRIVATE dwarfs_tool_support)
    endif()

    if(FUSE_FOUND AND (NOT APPLE) AND (WITH_LEGACY_FUSE OR NOT FUSE3_FOUND))
      add_library(dwarfs2_main OBJECT tools/src/dwarfs_main.cpp)
      target_compile_definitions(dwarfs2_main PRIVATE _FILE_OFFSET_BITS=64
                                                  FUSE_USE_VERSION=29)
      target_link_libraries(dwarfs2_main PRIVATE PkgConfig::FUSE)
      add_executable(dwarfs2-bin tools/src/dwarfs.cpp)
      target_link_libraries(dwarfs2-bin PRIVATE dwarfs2_main)
      if(TARGET dwarfsuniversal AND (NOT FUSE3_FOUND))
        target_link_libraries(dwarfsuniversal PRIVATE dwarfs2_main)
        target_compile_definitions(dwarfsuniversal PRIVATE DWARFS_UNIVERSAL_FUSE_DRIVER)
      endif()
      if(TARGET dwarfsfuseextract AND (NOT FUSE3_FOUND))
        target_link_libraries(dwarfsfuseextract PRIVATE dwarfs2_main)
        target_compile_definitions(dwarfsfuseextract PRIVATE DWARFS_UNIVERSAL_FUSE_DRIVER)
      endif()
      if(TARGET sfxstub_lz4 OR TARGET sfxstub_zstd_minimal)
        set(_stub_target sfxstub_lz4)
        if(TARGET sfxstub_zstd)
          set(_stub_target sfxstub_zstd)
        endif()
        install(TARGETS ${_stub_target} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
        if(MSVC)
          install(FILES $<$<CXX_COMPILER_ID:MSVC>:$<TARGET_PDB_FILE:${_stub_target}>> DESTINATION ${CMAKE_INSTALL_BINDIR} OPTIONAL)
        endif()
      endif()
      set_target_properties(dwarfs2-bin PROPERTIES OUTPUT_NAME dwarfs2)
      install(TARGETS dwarfs2-bin RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
      install(CODE "\
        file(MAKE_DIRECTORY \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_SBINDIR}\")
        file(CREATE_LINK \"${relative_sbindir_to_bindir}/dwarfs2\"
                         \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_SBINDIR}/mount.dwarfs2\"
                         SYMBOLIC)")
      list(APPEND BINARY_TARGETS dwarfs2-bin)
      list(APPEND MAIN_TARGETS dwarfs2_main)
      target_link_libraries(dwarfs2_main PRIVATE dwarfs_tool
                           PRIVATE dwarfs_tool_support)
    endif()
  endif()
endif()

# ============================================================================
# Manpage Integration (--man option)
# ============================================================================

if(WITH_MAN_OPTION)
  if(DWARFS_GIT_BUILD)
    include(${CMAKE_SOURCE_DIR}/cmake/render_manpage.cmake)
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/tools/src")
    set(DWARFS_MANPAGE_SOURCE_DIR ${CMAKE_CURRENT_BINARY_DIR})
  else()
    set(DWARFS_MANPAGE_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
  endif()

  foreach(man mkdwarfs dwarfs dwarfsck dwarfsextract)
    if(DWARFS_GIT_BUILD)
      file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/tools/src")
      add_manpage_source(doc/${man}.md NAME ${man}
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/tools/src/${man}_manpage.cpp)
    endif()

    if(TARGET ${man}_main)
      target_sources(${man}_main PRIVATE ${DWARFS_MANPAGE_SOURCE_DIR}/tools/src/${man}_manpage.cpp)
    endif()
  endforeach()

  if(TARGET dwarfs2_main)
    target_sources(dwarfs2_main PRIVATE ${DWARFS_MANPAGE_SOURCE_DIR}/tools/src/dwarfs_manpage.cpp)
  endif()
endif()

# ============================================================================
# Desktop Integration
# ============================================================================

if(WITH_DESKTOP_INTEGRATION AND NOT WIN32)
  install(FILES "data/dwarfs.xml" DESTINATION ${CMAKE_INSTALL_DATADIR}/mime/packages)
  if(WITH_FUSE_DRIVER)
    install(FILES "data/dwarfs-mount-handler.desktop" DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
  endif()
endif()