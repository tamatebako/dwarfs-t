/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \author     Ribose (@riboseinc @tamatebako)
 * \copyright  Copyright (c) Ribose
 */
#pragma once

#ifdef DWARFS_HAVE_THRIFT

#include "metadata_serializer.h"

namespace dwarfs::metadata::serialization {

/**
 * Thrift Compact Serializer (Legacy Wrapper)
 *
 * Wraps existing Apache Thrift frozen serialization code for
 * legacy compatibility. This serializer is READ-ONLY.
 *
 * Features:
 * - Legacy format compatibility
 * - Read-only (no new writes)
 * - Wraps existing Thrift frozen code
 * - Magic bytes: 0x82 0x21 (Thrift Compact Protocol)
 *
 * Note: This format is deprecated for new filesystems.
 * Use Cereal or Bitsery for new filesystems.
 */
class ThriftCompactSerializer : public IMetadataSerializer {
public:
  std::vector<uint8_t> serialize(const void* metadata) const override;

  std::unique_ptr<void, void(*)(void*)> deserialize(
      const std::vector<uint8_t>& data) const override;

  std::string_view get_format_name() const noexcept override {
    return "Thrift Compact";
  }

  SerializationFormat get_format() const noexcept override {
    return SerializationFormat::THRIFT_COMPACT;
  }

  bool can_write() const noexcept override {
    // Thrift is read-only for legacy compatibility
    return false;
  }

  bool can_read() const noexcept override { return true; }

  std::vector<uint8_t> get_magic_bytes() const noexcept override {
    return {MAGIC_BYTES[0], MAGIC_BYTES[1]};
  }

private:
  static constexpr std::array<uint8_t, 2> MAGIC_BYTES = {0x82, 0x21};
};

// Registration function to ensure static initializer runs
void register_thrift_serializer();

} // namespace dwarfs::metadata::serialization

#endif // DWARFS_HAVE_THRIFT