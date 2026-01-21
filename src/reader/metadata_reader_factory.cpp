/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \file metadata_reader_factory.cpp
 *
 * Metadata Reader Factory Implementation
 *
 * Format detection and reader creation. NO PREPROCESSOR GUARDS: CMake
 * controls which implementations are available via conditional compilation
 * of the implementation files.
 *
 * \author Ribose Inc.
 * \date 2025-12-22
 * \copyright See LICENSE file
 */

#include "dwarfs/reader/metadata_reader_interface.h"
#include <stdexcept>
#include <cstring>

#ifdef DWARFS_HAVE_EXPERIMENTAL_THRIFT
#include <dwarfs/gen-cpp2/metadata_types.h>
#endif

namespace dwarfs::reader {

// Forward declarations of factory functions (defined in implementation files)
// These are only linked if the format is enabled
#ifdef DWARFS_HAVE_FLATBUFFERS
std::unique_ptr<metadata_reader_interface>
create_flatbuffers_metadata_reader(const uint8_t* data, size_t size);
#endif

#ifdef DWARFS_HAVE_EXPERIMENTAL_THRIFT
std::unique_ptr<metadata_reader_interface>
create_thrift_metadata_reader(const dwarfs::thrift::metadata::metadata& thrift_meta);
#endif

/**
 * Detect metadata format from magic bytes
 *
 * @param data Metadata bytes
 * @param size Data size
 * @return Format name or empty string if unknown
 */
std::string detect_metadata_format(const uint8_t* data, size_t size) {
  if (size < 8) {
    return "";
  }

  // Check for FlatBuffers size-prefixed format
  // Format: [4-byte size][4-byte identifier "DFBF"]
  if (size >= 8) {
    const char* magic = reinterpret_cast<const char*>(data + 4);
    if (std::memcmp(magic, "DFBF", 4) == 0) {
      return "FlatBuffers";
    }
  }

  // Default to Thrift if no FlatBuffers magic found
  return "Thrift";
}

/**
 * Create appropriate metadata reader based on detected format
 *
 * @param data Metadata bytes
 * @param size Data size
 * @return Metadata reader instance
 * @throws std::runtime_error if format is unsupported or not compiled in
 */
std::unique_ptr<metadata_reader_interface>
create_metadata_reader(const uint8_t* data, size_t size) {
  std::string format = detect_metadata_format(data, size);

  if (format == "FlatBuffers") {
#ifdef DWARFS_HAVE_FLATBUFFERS
    return create_flatbuffers_metadata_reader(data, size);
#else
    throw std::runtime_error("FlatBuffers format detected but support not compiled in");
#endif
  }

  if (format == "Thrift" || format.empty()) {
#ifdef DWARFS_HAVE_EXPERIMENTAL_THRIFT
    // Note: This is a simple case - real integration needs Thrift deserialization first
    throw std::runtime_error("Thrift reader requires deserialized metadata object");
#else
    throw std::runtime_error("Thrift format detected but support not compiled in");
#endif
  }

  throw std::runtime_error("Unknown metadata format");
}

} // namespace dwarfs::reader