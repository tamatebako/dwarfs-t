# Metadata Serialization Configuration
#
# Configures support for multiple serialization formats:
# - Thrift Compact (legacy, optional for compatibility)
# - FlatBuffers (modern unified format, optional but recommended)
# - Cereal Binary (DEPRECATED, read-only for migration)
# - Bitsery (DEPRECATED, read-only for migration)

message(STATUS "Configuring metadata serialization formats...")

# Serialization format options
option(DWARFS_WITH_THRIFT "Enable Thrift serialization support (legacy)" ON)
option(DWARFS_WITH_FLATBUFFERS "Enable FlatBuffers serialization support (recommended)" ON)

# Force Thrift OFF in Tebako builds (static linking incompatible)
if(TEBAKO_BUILD)
  set(DWARFS_WITH_THRIFT OFF CACHE BOOL "Disable Thrift in Tebako builds" FORCE)
  message(STATUS "TEBAKO build detected - Thrift disabled, FlatBuffers enabled")
endif()

# At least one serialization format must be enabled
if(NOT DWARFS_WITH_THRIFT AND NOT DWARFS_WITH_FLATBUFFERS)
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
    set(FBS_GENERATED_HEADER ${FLATBUFFERS_GENERATED_DIR}/metadata_generated.h)

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
      $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
      $<INSTALL_INTERFACE:include>
    )

    # Link against FlatBuffers library
    if(TARGET flatbuffers::flatbuffers)
      target_link_libraries(dwarfs_metadata_flatbuffers INTERFACE flatbuffers::flatbuffers)
    elseif(TARGET flatbuffers)
      target_link_libraries(dwarfs_metadata_flatbuffers INTERFACE flatbuffers)
    else()
      message(FATAL_ERROR "FlatBuffers library target not found!")
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

# Thrift - Legacy format (optional, conditional compilation)
if(DWARFS_WITH_THRIFT)
  # Thrift support already configured via cmake/folly.cmake and cmake/thrift.cmake
  # We just need to check if it's available
  if(TARGET folly AND TARGET thrift1)
    add_compile_definitions(DWARFS_HAVE_THRIFT=1)
    set(DWARFS_HAVE_THRIFT ON)
    message(STATUS "Thrift serialization: ENABLED (legacy format support)")
  else()
    set(DWARFS_WITH_THRIFT OFF)
    set(DWARFS_HAVE_THRIFT OFF)
    message(STATUS "Thrift serialization: DISABLED (fbthrift not found)")
  endif()
else()
  set(DWARFS_HAVE_THRIFT OFF)
  message(STATUS "Thrift serialization: DISABLED (by option)")
endif()

# Verify at least one serialization format is available
if(NOT DWARFS_HAVE_FLATBUFFERS AND NOT DWARFS_HAVE_THRIFT)
  message(FATAL_ERROR "At least one serialization format must be enabled! Enable either FlatBuffers or Thrift.")
endif()

# Define serialization source files
set(SERIALIZATION_SOURCES
  src/metadata/serialization/serializer_registry.cpp
  src/metadata/serialization/init_serializers.cpp
  src/metadata/serialization/serialization_facade.cpp
  src/metadata/serialization/facade_factory.cpp
)

if(DWARFS_HAVE_FLATBUFFERS)
  list(APPEND SERIALIZATION_SOURCES
    src/metadata/serialization/flatbuffers_serializer.cpp
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
    src/metadata/serialization/thrift_compact_serializer.cpp
  )
endif()

# Export sources for use in main CMakeLists.txt
set(DWARFS_SERIALIZATION_SOURCES ${SERIALIZATION_SOURCES} CACHE INTERNAL "Serialization source files")

# Summary
message(STATUS "════════════════════════════════════════════════════")
message(STATUS "Metadata serialization summary:")
message(STATUS "  - FlatBuffers:    ${DWARFS_HAVE_FLATBUFFERS} (modern recommended)")
message(STATUS "  - Thrift Compact: ${DWARFS_HAVE_THRIFT} (legacy, optional)")
message(STATUS "════════════════════════════════════════════════════")