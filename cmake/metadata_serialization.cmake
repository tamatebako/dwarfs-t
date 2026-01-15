# DwarFS Metadata Serialization Configuration
#
# Supported Formats:
# - Legacy Thrift (ALWAYS enabled, hand-coded, no dependencies)
# - FlatBuffers (default ON, can be disabled)
# - Modern Thrift Compact (default OFF, requires fbthrift)

message(STATUS "Configuring metadata serialization formats...")

# Serialization format options
option(DWARFS_WITH_THRIFT "Enable Modern Thrift serialization (requires fbthrift)" OFF)
option(DWARFS_WITH_FLATBUFFERS "Enable FlatBuffers serialization (recommended)" ON)

# Legacy Thrift is ALWAYS available (no external dependencies, cannot be disabled)
set(DWARFS_HAVE_LEGACY_THRIFT ON)

# Force Modern Thrift OFF in Tebako builds (static linking incompatible)
if(TEBAKO_BUILD)
  set(DWARFS_WITH_THRIFT OFF CACHE BOOL "Disable Modern Thrift in Tebako builds" FORCE)
  message(STATUS "TEBAKO build detected - Modern Thrift disabled, FlatBuffers enabled")
endif()

# At least one serialization format must be enabled
# Note: Legacy Thrift is always available, so this is always satisfied
if(NOT DWARFS_WITH_THRIFT AND NOT DWARFS_WITH_FLATBUFFERS AND NOT DWARFS_HAVE_LEGACY_THRIFT)
  message(FATAL_ERROR "At least one serialization format must be enabled! Enable either FlatBuffers or Thrift.")
endif()

# FlatBuffers - Modern unified serialization format (optional but recommended)
if(DWARFS_WITH_FLATBUFFERS)
  include(${CMAKE_SOURCE_DIR}/cmake/need_flatbuffers.cmake)

  if(TARGET flatbuffers::flatc OR TARGET flatc)
    # Define output directory for generated headers
    # Must create subdirectory structure matching include path: dwarfs/gen-flatbuffers/
    set(FLATBUFFERS_GENERATED_DIR ${CMAKE_CURRENT_BINARY_DIR}/include/dwarfs/gen-flatbuffers)
    file(MAKE_DIRECTORY ${FLATBUFFERS_GENERATED_DIR})

    # Compile the FlatBuffers schema
    set(FBS_SCHEMA ${CMAKE_SOURCE_DIR}/flatbuffers/metadata.fbs)
    set(FBS_GENERATED_HEADER ${FLATBUFFERS_GENERATED_DIR}/metadata.h)

    # Determine flatc executable - use generator expression for built target
    if(TARGET flatc)
      set(FLATC_EXE $<TARGET_FILE:flatc>)
      set(FLATC_DEP flatc)
    elseif(TARGET flatbuffers::flatc)
      get_target_property(FLATC_EXE flatbuffers::flatc IMPORTED_LOCATION)
      if(NOT FLATC_EXE)
        # Try IMPORTED_LOCATION_<CONFIG>
        get_target_property(FLATC_EXE flatbuffers::flatc IMPORTED_LOCATION_RELEASE)
      endif()
      set(FLATC_DEP flatbuffers::flatc)
    else()
      message(FATAL_ERROR "FlatBuffers compiler (flatc) not found!")
    endif()

    # Generate the header file from the schema
    add_custom_command(
      OUTPUT ${FBS_GENERATED_HEADER}
      COMMAND ${FLATC_EXE} --cpp --scoped-enums --gen-object-api
              --cpp-std c++17 --filename-suffix "" --filename-ext h
              -o ${FLATBUFFERS_GENERATED_DIR}
              ${FBS_SCHEMA}
      DEPENDS ${FBS_SCHEMA} ${FLATC_DEP}
      COMMENT "Generating FlatBuffers header from ${FBS_SCHEMA}"
      VERBATIM
    )

    # Compile the history.fbs schema
    set(FBS_HISTORY_SCHEMA ${CMAKE_SOURCE_DIR}/flatbuffers/history.fbs)
    set(FBS_HISTORY_HEADER ${FLATBUFFERS_GENERATED_DIR}/history_generated.h)

    add_custom_command(
      OUTPUT ${FBS_HISTORY_HEADER}
      COMMAND ${FLATC_EXE} --cpp --scoped-enums --gen-object-api
              --cpp-std c++17
              -o ${FLATBUFFERS_GENERATED_DIR}
              ${FBS_HISTORY_SCHEMA}
      DEPENDS ${FBS_HISTORY_SCHEMA} ${FLATC_DEP}
      COMMENT "Generating FlatBuffers header from ${FBS_HISTORY_SCHEMA}"
      VERBATIM
    )

    # Create a custom target for the generated headers
    add_custom_target(dwarfs_metadata_flatbuffers_generate
      DEPENDS ${FBS_GENERATED_HEADER} ${FBS_HISTORY_HEADER}
    )

    # Create an interface library for the generated FlatBuffers code
    add_library(dwarfs_metadata_flatbuffers INTERFACE)
    add_dependencies(dwarfs_metadata_flatbuffers dwarfs_metadata_flatbuffers_generate)
    target_include_directories(dwarfs_metadata_flatbuffers INTERFACE
      $<BUILD_INTERFACE:${FLATBUFFERS_GENERATED_DIR}/../..>  # For dwarfs/gen-flatbuffers/... (build/include)
      $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    )

    # Link flatbuffers for include directories only
    # Don't propagate to consuming projects (header-only)
    if(TARGET flatbuffers::flatbuffers)
      target_include_directories(dwarfs_metadata_flatbuffers INTERFACE
        $<TARGET_PROPERTY:flatbuffers::flatbuffers,INTERFACE_INCLUDE_DIRECTORIES>
      )
    elseif(TARGET flatbuffers)
      target_include_directories(dwarfs_metadata_flatbuffers INTERFACE
        $<TARGET_PROPERTY:flatbuffers,INTERFACE_INCLUDE_DIRECTORIES>
      )
    endif()

    # Set variables for use in other parts of the build system
    set(DWARFS_HAVE_FLATBUFFERS ON)
    add_compile_definitions(DWARFS_HAVE_FLATBUFFERS=1)
    message(STATUS "FlatBuffers serialization: ENABLED (modern default)")
  else()
    set(DWARFS_WITH_FLATBUFFERS OFF)
    set(DWARFS_HAVE_FLATBUFFERS OFF)
    message(STATUS "FlatBuffers serialization: DISABLED (flatbuffers not found)")
  endif()
else()
  set(DWARFS_HAVE_FLATBUFFERS OFF)
  message(STATUS "FlatBuffers serialization: DISABLED (by option)")
endif()

# Legacy Thrift - Hand-coded implementation (always available, no external deps)
# This provides backward compatibility with v0.14.1 without fbthrift dependency
set(LEGACY_THRIFT_SOURCES
  src/metadata/legacy/frozen_bits.cpp
  src/metadata/legacy/thrift_types.cpp
  src/metadata/legacy/thrift_compact_writer.cpp
  src/metadata/legacy/thrift_compact_reader.cpp
  src/metadata/legacy/legacy_metadata_serializer.cpp

  # Frozen2 schema system (Session 77)
  src/metadata/legacy/frozen_schema.cpp
  src/metadata/legacy/frozen_schema_serializer.cpp
  src/metadata/legacy/frozen_bit_writer.cpp
  src/metadata/legacy/frozen2_serializer.cpp
  src/metadata/legacy/frozen2_deserializer.cpp

  # Frozen2 modular implementation (Session 79)
  src/metadata/legacy/frozen2_layout.cpp
  src/metadata/legacy/frozen2_layout_builder.cpp
  src/metadata/legacy/frozen2_schema_converter.cpp
  src/metadata/legacy/frozen2_value_serializer.cpp

  # Frozen2 Value Encoder hierarchy (Task 1-2 of Frozen2 Serializer Implementation)
  src/metadata/legacy/value_encoders.cpp
  src/metadata/legacy/frozen_writer.cpp

  # Frozen2 SchemaBuilder (Task 4 of Frozen2 Serializer Implementation)
  src/metadata/legacy/frozen2_schema_builder.cpp

  # FSST decompression support (Session 84)
  src/metadata/legacy/fsst.cpp
)

add_library(dwarfs_metadata_legacy ${LEGACY_THRIFT_SOURCES})

target_include_directories(dwarfs_metadata_legacy
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/fsst>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_compile_features(dwarfs_metadata_legacy
  PUBLIC cxx_std_20
)

target_compile_definitions(dwarfs_metadata_legacy
  PUBLIC DWARFS_HAVE_LEGACY_THRIFT=1
)

# Link fmt for error messages
target_link_libraries(dwarfs_metadata_legacy
  PUBLIC fmt::fmt
)

message(STATUS "Legacy Thrift: ENABLED (hand-coded, no external deps)")

# Modern Thrift Compact - Uses apache::thrift::CompactSerializer from fbthrift
if(DWARFS_WITH_THRIFT)
  # Try to find fbthrift (folly is a dependency of fbthrift)
  find_package(folly CONFIG)
  find_package(FBThrift CONFIG)

  if(folly_FOUND AND FBThrift_FOUND)
    set(DWARFS_HAVE_THRIFT ON)
    add_compile_definitions(DWARFS_HAVE_THRIFT=1)
    add_compile_definitions(DWARFS_HAVE_MODERN_THRIFT=1)

    # Modern Thrift sources (to be implemented in Sessions 87-88)
    set(THRIFT_MODERN_IDL ${CMAKE_SOURCE_DIR}/thrift/metadata_modern.thrift)
    set(THRIFT_MODERN_BUILD_DIR ${CMAKE_CURRENT_BINARY_DIR}/thrift/modern)
    set(THRIFT_MODERN_IDL_COPY ${THRIFT_MODERN_BUILD_DIR}/metadata_modern.thrift)
    set(THRIFT_MODERN_GEN_DIR ${THRIFT_MODERN_BUILD_DIR}/gen-cpp2)

    # All generated Thrift C++ files
    set(THRIFT_MODERN_GENERATED_FILES
      ${THRIFT_MODERN_GEN_DIR}/metadata_modern_types.h
      ${THRIFT_MODERN_GEN_DIR}/metadata_modern_types.cpp
      ${THRIFT_MODERN_GEN_DIR}/metadata_modern_types.tcc
      ${THRIFT_MODERN_GEN_DIR}/metadata_modern_types_fwd.h
      ${THRIFT_MODERN_GEN_DIR}/metadata_modern_types_custom_protocol.h
      ${THRIFT_MODERN_GEN_DIR}/metadata_modern_data.h
      ${THRIFT_MODERN_GEN_DIR}/metadata_modern_data.cpp
      ${THRIFT_MODERN_GEN_DIR}/metadata_modern_constants.h
      ${THRIFT_MODERN_GEN_DIR}/metadata_modern_constants.cpp
      ${THRIFT_MODERN_GEN_DIR}/metadata_modern_types_binary.cpp
      ${THRIFT_MODERN_GEN_DIR}/metadata_modern_types_compact.cpp
      ${THRIFT_MODERN_GEN_DIR}/metadata_modern_types_serialization.cpp
      ${THRIFT_MODERN_GEN_DIR}/metadata_modern_clients.h
      ${THRIFT_MODERN_GEN_DIR}/metadata_modern_handlers.h
      ${THRIFT_MODERN_GEN_DIR}/metadata_modern_metadata.h
      ${THRIFT_MODERN_GEN_DIR}/metadata_modern_sinit.cpp
      ${THRIFT_MODERN_GEN_DIR}/metadata_modern_for_each_field.h
      ${THRIFT_MODERN_GEN_DIR}/metadata_modern_visit_union.h
      ${THRIFT_MODERN_GEN_DIR}/metadata_modern_visitation.h
      ${THRIFT_MODERN_GEN_DIR}/metadata_modern_visit_by_thrift_field_metadata.h
    )

    # Determine thrift1 include path (same as thrift_library.cmake)
    if(VCPKG_INSTALLED_DIR AND VCPKG_TARGET_TRIPLET)
      set(_THRIFT_MODERN_INCLUDE_PATH "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/include")
    elseif(_VCPKG_INSTALLED_DIR AND VCPKG_TARGET_TRIPLET)
      set(_THRIFT_MODERN_INCLUDE_PATH "${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/include")
    else()
      set(_THRIFT_MODERN_INCLUDE_PATH "${CMAKE_SOURCE_DIR}")
    endif()

    # Create directory marker (similar to _keep_${_THRIFTNAME} in thrift_library.cmake)
    add_custom_command(
      OUTPUT ${THRIFT_MODERN_BUILD_DIR}/_keep_metadata_modern
      COMMAND ${CMAKE_COMMAND} -E make_directory ${THRIFT_MODERN_BUILD_DIR}
      COMMAND ${CMAKE_COMMAND} -E touch ${THRIFT_MODERN_BUILD_DIR}/_keep_metadata_modern
    )

    # Generate C++ code from Thrift IDL using fbthrift compiler
    # Use change-directory pattern like thrift_library.cmake for relative includes
    add_custom_command(
      OUTPUT ${THRIFT_MODERN_GENERATED_FILES}
      # Copy .thrift file to build subdirectory
      COMMAND ${CMAKE_COMMAND} -E copy ${THRIFT_MODERN_IDL} ${THRIFT_MODERN_BUILD_DIR}/metadata_modern.thrift
      # Run thrift1 compiler with relative path (WORKING_DIRECTORY ensures relative includes)
      COMMAND ${CMAKE_COMMAND} -E env ASAN_OPTIONS=detect_leaks=0
              ${THRIFT1_COMPILER}
              -I ${_THRIFT_MODERN_INCLUDE_PATH}
              -o ${THRIFT_MODERN_BUILD_DIR}
              --gen mstch_cpp2:no_metadata
              metadata_modern.thrift
      DEPENDS ${THRIFT_MODERN_IDL}
              ${THRIFT_MODERN_BUILD_DIR}/_keep_metadata_modern
      WORKING_DIRECTORY ${THRIFT_MODERN_BUILD_DIR}
      COMMENT "Generating Modern Thrift C++ types from ${THRIFT_MODERN_IDL}"
      VERBATIM
    )

    # Copy generated headers to expected location (include/dwarfs/gen-cpp2/)
    # This is needed because the source headers use #include <dwarfs/gen-cpp2/...>
    set(_MODERN_THRIFT_PUBLIC_HEADERS
      ${THRIFT_MODERN_GEN_DIR}/metadata_modern_types.h
      ${THRIFT_MODERN_GEN_DIR}/metadata_modern_types_fwd.h
      ${THRIFT_MODERN_GEN_DIR}/metadata_modern_types_custom_protocol.h
      ${THRIFT_MODERN_GEN_DIR}/metadata_modern_types.tcc
      ${THRIFT_MODERN_GEN_DIR}/metadata_modern_data.h
      ${THRIFT_MODERN_GEN_DIR}/metadata_modern_constants.h
    )

    set(_MODERN_THRIFT_COPIED_HEADERS)
    foreach(_header ${_MODERN_THRIFT_PUBLIC_HEADERS})
      get_filename_component(_header_name ${_header} NAME)
      set(_copied_header ${CMAKE_BINARY_DIR}/include/dwarfs/gen-cpp2/${_header_name})
      list(APPEND _MODERN_THRIFT_COPIED_HEADERS ${_copied_header})
      add_custom_command(
        OUTPUT ${_copied_header}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/include/dwarfs/gen-cpp2
        COMMAND ${CMAKE_COMMAND} -E copy ${_header} ${_copied_header}
        DEPENDS ${_header}
        COMMENT "Copying ${_header_name} to include/dwarfs/gen-cpp2/"
      )
    endforeach()

    # Create target for generated Thrift code (including copied headers)
    add_custom_target(dwarfs_metadata_modern_thrift_generate
      DEPENDS ${THRIFT_MODERN_GENERATED_FILES} ${_MODERN_THRIFT_COPIED_HEADERS}
    )

    # Modern Thrift converter and serializer sources
    set(MODERN_THRIFT_SOURCES
      # Generated Thrift types (.cpp files only)
      ${THRIFT_MODERN_GEN_DIR}/metadata_modern_types.cpp
      ${THRIFT_MODERN_GEN_DIR}/metadata_modern_data.cpp
      ${THRIFT_MODERN_GEN_DIR}/metadata_modern_constants.cpp
      ${THRIFT_MODERN_GEN_DIR}/metadata_modern_types_binary.cpp
      ${THRIFT_MODERN_GEN_DIR}/metadata_modern_types_compact.cpp
      ${THRIFT_MODERN_GEN_DIR}/metadata_modern_types_serialization.cpp

      # Session 87: Type converters
      src/metadata/modern/domain_to_thrift.cpp
      src/metadata/modern/thrift_to_domain.cpp

      # Session 88: Serializer implementation
      src/metadata/serialization/thrift_compact_serializer.cpp
    )

    # Create library for Modern Thrift components
    add_library(dwarfs_metadata_modern_thrift ${MODERN_THRIFT_SOURCES})
    add_dependencies(dwarfs_metadata_modern_thrift dwarfs_metadata_modern_thrift_generate)

    target_include_directories(dwarfs_metadata_modern_thrift
      PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}>                   # For config.h
        $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/include>           # For dwarfs/gen-cpp2/ mapping
        $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/include/dwarfs/gen-cpp2>  # For #include "metadata_modern_types.h"
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    )

    target_link_libraries(dwarfs_metadata_modern_thrift
      PUBLIC
        FBThrift::thriftcpp2
        Folly::folly
        fmt::fmt
    )

    target_compile_features(dwarfs_metadata_modern_thrift
      PUBLIC cxx_std_20
    )

    # Note: Modern Thrift library will be created in Session 87-88
    # For now, just track that the option is enabled

    message(STATUS "Modern Thrift Compact: ENABLED (using fbthrift ${FBThrift_VERSION})")
    message(STATUS "  - Folly version: ${folly_VERSION}")
    message(STATUS "  - Implementation: Pending (Sessions 87-88)")
  else()
    set(DWARFS_HAVE_THRIFT OFF)
    if(NOT folly_FOUND)
      message(STATUS "Modern Thrift Compact: DISABLED (folly not found)")
    endif()
    if(NOT FBThrift_FOUND)
      message(STATUS "Modern Thrift Compact: DISABLED (fbthrift not found)")
    endif()
  endif()
else()
  set(DWARFS_HAVE_THRIFT OFF)
  message(STATUS "Modern Thrift Compact: DISABLED (by option)")
endif()

# Tests for legacy thrift
if(WITH_TESTS)
  add_executable(legacy_thrift_tests
    test/metadata/legacy/thrift_compact_test.cpp
  )

  target_link_libraries(legacy_thrift_tests
    PRIVATE
      dwarfs_metadata_legacy
      GTest::gtest_main
      GTest::gmock
  )

  # Use add_test instead of gtest_discover_tests (which requires GoogleTest module)
  add_test(
    NAME legacy_thrift_tests
    COMMAND legacy_thrift_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  )

  set_tests_properties(legacy_thrift_tests
    PROPERTIES LABELS "legacy;metadata"
  )

  # Frozen bits tests
  add_executable(frozen_bits_tests
    test/metadata/legacy/frozen_bits_test.cpp
  )

  target_link_libraries(frozen_bits_tests
    PRIVATE
      dwarfs_metadata_legacy
      GTest::gtest_main
      GTest::gmock
  )

  add_test(
    NAME frozen_bits_tests
    COMMAND frozen_bits_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  )

  set_tests_properties(frozen_bits_tests
    PROPERTIES LABELS "legacy;metadata;frozen2"
  )

  # Metadata serializer round-trip tests
  add_executable(metadata_serializer_tests
    test/metadata/legacy/metadata_serializer_test.cpp
  )

  target_link_libraries(metadata_serializer_tests
    PRIVATE
      dwarfs_metadata_legacy
      GTest::gtest_main
      GTest::gmock
  )

  add_test(
    NAME metadata_serializer_tests
    COMMAND metadata_serializer_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  )

  set_tests_properties(metadata_serializer_tests
    PROPERTIES LABELS "legacy;metadata;serializer"
  )

  # Serialization registry integration tests
  add_executable(serialization_registry_tests
    test/metadata/serialization_registry_test.cpp
  )

  target_link_libraries(serialization_registry_tests
    PRIVATE
      dwarfs_common
      dwarfs_metadata_legacy
      GTest::gtest_main
      GTest::gmock
  )

  # Link FlatBuffers if available
  if(DWARFS_HAVE_FLATBUFFERS)
    target_link_libraries(serialization_registry_tests
      PRIVATE dwarfs_metadata_flatbuffers
    )
  endif()

  add_test(
    NAME serialization_registry_tests
    COMMAND serialization_registry_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  )

  set_tests_properties(serialization_registry_tests
    PROPERTIES LABELS "metadata;serialization;registry;integration"
  )

  # Frozen2 serializer tests (Session 80)
  add_executable(frozen2_serializer_tests
    test/metadata/legacy/frozen2_serializer_test.cpp
  )

  target_link_libraries(frozen2_serializer_tests
    PRIVATE
      dwarfs_metadata_legacy
      GTest::gtest_main
      GTest::gmock
  )

  add_test(
    NAME frozen2_serializer_tests
    COMMAND frozen2_serializer_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  )

  set_tests_properties(frozen2_serializer_tests
    PROPERTIES LABELS "legacy;metadata;frozen2;serializer"
  )

  # Value encoder tests (Task 1 of Frozen2 Serializer Implementation)
  add_executable(value_encoders_tests
    test/metadata/legacy/value_encoders_test.cpp
  )

  target_link_libraries(value_encoders_tests
    PRIVATE
      dwarfs_metadata_legacy
      GTest::gtest_main
      GTest::gmock
  )

  add_test(
    NAME value_encoders_tests
    COMMAND value_encoders_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  )

  set_tests_properties(value_encoders_tests
    PROPERTIES LABELS "legacy;metadata;frozen2;encoder"
  )

  # FrozenWriter tests (Task 2 of Frozen2 Serializer Implementation)
  add_executable(frozen_writer_tests
    test/metadata/legacy/frozen_writer_test.cpp
  )

  target_link_libraries(frozen_writer_tests
    PRIVATE
      dwarfs_metadata_legacy
      GTest::gtest_main
      GTest::gmock
  )

  add_test(
    NAME frozen_writer_tests
    COMMAND frozen_writer_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  )

  set_tests_properties(frozen_writer_tests
    PROPERTIES LABELS "legacy;metadata;frozen2;writer"
  )

  # SchemaBuilder tests (Task 4 of Frozen2 Serializer Implementation)
  add_executable(frozen2_schema_builder_tests
    test/metadata/legacy/frozen2_schema_builder_test.cpp
  )

  target_link_libraries(frozen2_schema_builder_tests
    PRIVATE
      dwarfs_metadata_legacy
      GTest::gtest_main
      GTest::gmock
  )

  add_test(
    NAME frozen2_schema_builder_tests
    COMMAND frozen2_schema_builder_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  )

  set_tests_properties(frozen2_schema_builder_tests
    PROPERTIES LABELS "legacy;metadata;frozen2;schema"
  )

  # Frozen2 integration tests (Task 10 - Final Integration Testing)
  add_executable(frozen2_integration_test
    test/metadata/legacy/frozen2_integration_test.cpp
  )

  target_link_libraries(frozen2_integration_test
    PRIVATE
      dwarfs_metadata_legacy
      GTest::gtest_main
      GTest::gmock
  )

  add_test(
    NAME frozen2_integration_test
    COMMAND frozen2_integration_test
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  )

  set_tests_properties(frozen2_integration_test
    PROPERTIES LABELS "legacy;metadata;frozen2;integration;serializer"
  )

  # Modern Thrift Compact serializer tests (if Thrift available)
  if(DWARFS_HAVE_THRIFT)
    # Modern Thrift converter tests
    add_executable(modern_thrift_converter_tests
      test/metadata/modern/converter_test.cpp
    )

    target_link_libraries(modern_thrift_converter_tests
      PRIVATE
        dwarfs_metadata_modern_thrift
        GTest::gtest_main
        GTest::gmock
    )

    add_test(
      NAME modern_thrift_converter_tests
      COMMAND modern_thrift_converter_tests
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )

    set_tests_properties(modern_thrift_converter_tests
      PROPERTIES LABELS "metadata;modern;thrift;converter"
    )

    # Modern Thrift serialization tests
    add_executable(modern_thrift_serialization_tests
      test/metadata/modern_thrift_serialization_test.cpp
    )

    target_link_libraries(modern_thrift_serialization_tests
      PRIVATE
        dwarfs_common
        dwarfs_metadata_legacy
        dwarfs_metadata_modern_thrift
        GTest::gtest_main
        GTest::gmock
    )

    add_test(
      NAME modern_thrift_serialization_tests
      COMMAND modern_thrift_serialization_tests
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )

    set_tests_properties(modern_thrift_serialization_tests
      PROPERTIES LABELS "metadata;serialization;thrift;modern"
    )
  endif()
endif()

# Verify at least one serialization format is available
# Note: Legacy Thrift is always available (no dependencies)
if(NOT DWARFS_HAVE_FLATBUFFERS AND NOT DWARFS_HAVE_THRIFT AND NOT DWARFS_HAVE_LEGACY_THRIFT)
  message(FATAL_ERROR "At least one serialization format must be enabled! Enable either FlatBuffers or Thrift.")
endif()

# Define serialization source files
set(SERIALIZATION_SOURCES
  src/metadata/serialization/serializer_registry.cpp
  src/metadata/serialization/init_serializers.cpp
  src/metadata/serialization/serialization_facade.cpp
  src/metadata/serialization/facade_factory.cpp
  src/metadata/serialization/legacy_thrift_serializer.cpp
)

if(DWARFS_HAVE_FLATBUFFERS)
  list(APPEND SERIALIZATION_SOURCES
    src/metadata/serialization/flatbuffers_serializer.cpp
    src/metadata/converters/domain_flatbuffers_converter.cpp
  )
endif()

# C++ to Thrift converter (always compiled, properly guarded)
list(APPEND SERIALIZATION_SOURCES
  src/metadata/converters/cpp_thrift_converter.cpp
)

if(DWARFS_HAVE_THRIFT)
  # NEW: Domain model to Thrift converter
  list(APPEND SERIALIZATION_SOURCES
    src/metadata/converters/domain_thrift_converter.cpp
  )
endif()

if(DWARFS_HAVE_THRIFT)
  list(APPEND SERIALIZATION_SOURCES
    src/metadata/converters/thrift_metadata_converter.cpp
  )
endif()

# Export sources for use in main CMakeLists.txt
set(DWARFS_SERIALIZATION_SOURCES ${SERIALIZATION_SOURCES} CACHE INTERNAL "Serialization source files")

# Summary
message(STATUS "════════════════════════════════════════════════════")
message(STATUS "Metadata serialization summary:")
message(STATUS "  - Legacy Thrift:  ON (hand-coded, always available, cannot be disabled)")
message(STATUS "  - FlatBuffers:    ${DWARFS_HAVE_FLATBUFFERS} (modern default, use -DDWARFS_WITH_FLATBUFFERS=OFF to disable)")
message(STATUS "  - Modern Thrift:  ${DWARFS_HAVE_THRIFT} (fbthrift v2025.12.29.00+, use -DDWARFS_WITH_THRIFT=ON to enable)")
message(STATUS "════════════════════════════════════════════════════")
message(STATUS "Build configurations:")
message(STATUS "  1. Default:          FlatBuffers + Legacy Thrift")
message(STATUS "  2. With Modern:      FlatBuffers + Legacy Thrift + Modern Thrift")
message(STATUS "════════════════════════════════════════════════════")