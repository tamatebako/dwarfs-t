/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \author     Ribose (@riboseinc @tamatebako)
 * \copyright  Copyright (c) Ribose
 */
#pragma once

#ifdef DWARFS_HAVE_FLATBUFFERS

#include "metadata_serializer.h"
#include "../domain/metadata.h"

namespace dwarfs::metadata::serialization {

/**
 * FlatBuffers Serializer
 *
 * Implements metadata serialization using Google FlatBuffers.
 * This is the modern unified format replacing Cereal and Bitsery.
 *
 * Features:
 * - Zero-copy memory-mapped access
 * - Excellent cross-platform support
 * - Industry-standard format
 * - Read and write support
 * - File identifier: "DFBF" (DwarFS FlatBuffers)
 */
class FlatBuffersSerializer : public IMetadataSerializer {
public:
  std::vector<uint8_t> serialize(const void* metadata) const override;

  std::unique_ptr<void, void(*)(void*)> deserialize(
      const std::vector<uint8_t>& data) const override;

  std::string_view get_format_name() const noexcept override {
    return "FlatBuffers";
  }

  SerializationFormat get_format() const noexcept override {
    return SerializationFormat::FLATBUFFERS;
  }

  bool can_write() const noexcept override { return true; }
  bool can_read() const noexcept override { return true; }

  std::vector<uint8_t> get_magic_bytes() const noexcept override {
    return {MAGIC_BYTES[0], MAGIC_BYTES[1], MAGIC_BYTES[2], MAGIC_BYTES[3]};
  }

private:
  static constexpr std::array<uint8_t, 4> MAGIC_BYTES = {'D', 'F', 'B', 'F'};
};

// Registration function to ensure static initializer runs
void register_flatbuffers_serializer();

} // namespace dwarfs::metadata::serialization

#endif // DWARFS_HAVE_FLATBUFFERS