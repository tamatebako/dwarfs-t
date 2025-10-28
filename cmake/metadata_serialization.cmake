# CMake configuration for metadata serialization layer
#
# This module provides options for configuring metadata serialization support,
# including optional legacy Thrift format compatibility.

# Option to enable legacy Thrift support for backward compatibility
option(DWARFS_LEGACY_THRIFT_SUPPORT
  "Enable legacy Apache Thrift serialization format support for reading old .dwarfs files"
  ON)

# Display configuration
message(STATUS "DwarFS Metadata Serialization Configuration:")
message(STATUS "  Legacy Thrift Support: ${DWARFS_LEGACY_THRIFT_SUPPORT}")

# Configure compile definitions
if(DWARFS_LEGACY_THRIFT_SUPPORT)
  add_compile_definitions(DWARFS_LEGACY_THRIFT_SUPPORT)
  message(STATUS "  - ThriftCompactSerializer will be available")
  message(STATUS "  - Can read legacy .dwarfs files created with Thrift format")
else()
  message(STATUS "  - ThriftCompactSerializer will NOT be available")
  message(STATUS "  - Legacy .dwarfs files cannot be read")
  message(STATUS "  - Build size will be smaller without Thrift dependencies")
endif()

# Function to add metadata serialization sources to a target
function(target_add_metadata_serialization target_name)
  # Always include core serialization sources
  target_sources(${target_name} PRIVATE
    ${CMAKE_SOURCE_DIR}/src/metadata/serialization/cereal_binary_serializer.cpp
  )

  # Conditionally include Thrift serialization sources
  if(DWARFS_LEGACY_THRIFT_SUPPORT)
    target_sources(${target_name} PRIVATE
      ${CMAKE_SOURCE_DIR}/src/metadata/serialization/thrift_converter.cpp
      ${CMAKE_SOURCE_DIR}/src/metadata/serialization/thrift_compact_serializer.cpp
    )
  endif()
endfunction()

# Function to link required libraries for metadata serialization
function(target_link_metadata_serialization target_name)
  # Always link Cereal (header-only, no actual linking needed)
  # Cereal is included via domain model headers

  # Conditionally link Thrift libraries
  if(DWARFS_LEGACY_THRIFT_SUPPORT)
    # Link Apache Thrift libraries
    target_link_libraries(${target_name} PRIVATE
      FBThrift::thrift
      Folly::folly
    )
  endif()
endfunction()