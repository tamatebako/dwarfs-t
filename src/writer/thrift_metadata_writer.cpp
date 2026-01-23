/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \file thrift_metadata_writer.cpp
 *
 * Modern Thrift Metadata Writer Implementation
 *
 * This file is ONLY compiled when DWARFS_WITH_EXPERIMENTAL_THRIFT=ON
 * via CMake configuration (requires fbthrift).
 *
 * \author Ribose Inc.
 * \date 2025-12-22
 * \copyright See LICENSE file
 */

#include "dwarfs/writer/metadata_writer_interface.h"
#include "dwarfs/metadata/converters/domain_thrift_converter.h"
#include "dwarfs/malloc_byte_buffer.h"
#include <dwarfs/gen-cpp2/metadata_types.h>
#include <folly/io/IOBuf.h>
#include <thrift/lib/cpp2/protocol/Serializer.h>

namespace dwarfs::writer {

class thrift_metadata_writer : public metadata_writer_interface {
public:
  thrift_metadata_writer() = default;

  mutable_byte_buffer serialize(const metadata::domain::metadata& meta) override {
    // Convert domain model to Thrift
    auto thrift_meta = metadata::converters::to_thrift(meta);

    // Serialize using Thrift Compact protocol
    auto serialized = apache::thrift::CompactSerializer::serialize<std::string>(thrift_meta);

    // Create mutable byte buffer from serialized data
    return malloc_byte_buffer::create(
        std::span<uint8_t const>{
            reinterpret_cast<const uint8_t*>(serialized.data()),
            serialized.size()
        });
  }

  std::string_view get_format_name() const override {
    return "Thrift";
  }
};

// Factory function (will be called by factory)
std::unique_ptr<metadata_writer_interface>
create_thrift_metadata_writer() {
  return std::make_unique<thrift_metadata_writer>();
}

} // namespace dwarfs::writer