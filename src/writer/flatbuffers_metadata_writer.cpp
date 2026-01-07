/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \file flatbuffers_metadata_writer.cpp
 *
 * FlatBuffers Metadata Writer Implementation
 *
 * This file is part of the FlatBuffers metadata support, which is
 * always enabled as the default metadata format.
 *
 * \author Ribose Inc.
 * \date 2025-12-22
 * \copyright See LICENSE file
 */

#include "dwarfs/writer/metadata_writer_interface.h"
#include "dwarfs/metadata/converters/domain_flatbuffers_converter.h"
#include "dwarfs/malloc_byte_buffer.h"
#include <dwarfs/gen-flatbuffers/metadata.h>
#include <flatbuffers/flatbuffers.h>

namespace dwarfs::writer {

class flatbuffers_metadata_writer : public metadata_writer_interface {
public:
  flatbuffers_metadata_writer() = default;

  mutable_byte_buffer serialize(const metadata::domain::metadata& meta) override {
    ::flatbuffers::FlatBufferBuilder builder(1024 * 1024); // 1 MB initial

    // Convert domain model to FlatBuffers
    auto metadata_offset = metadata::converters::to_flatbuffers(builder, meta);

    // Finish with size prefix and file identifier
    builder.FinishSizePrefixed(metadata_offset, "DFBF");

    // Create byte buffer from FlatBuffers data
    auto size = builder.GetSize();
    auto data = builder.GetBufferPointer();

    return malloc_byte_buffer::create(std::span<uint8_t const>{data, size});
  }

  std::string_view get_format_name() const override {
    return "FlatBuffers";
  }
};

// Factory function (will be called by factory)
std::unique_ptr<metadata_writer_interface>
create_flatbuffers_metadata_writer() {
  return std::make_unique<flatbuffers_metadata_writer>();
}

} // namespace dwarfs::writer