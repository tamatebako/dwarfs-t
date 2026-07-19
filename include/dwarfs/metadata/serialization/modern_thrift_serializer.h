/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \author     Marcus Holland-Moritz (github@mhxnet.de)
 * \copyright  Copyright (c) Marcus Holland-Moritz
 *
 * This file is part of dwarfs.
 *
 * dwarfs is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * dwarfs is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with dwarfs.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#ifdef DWARFS_HAVE_EXPERIMENTAL_THRIFT

#include "metadata_serializer.h"
#include "../domain/metadata.h"

namespace dwarfs::metadata::serialization {

/**
 * Modern Thrift CompactProtocol Serializer
 *
 * Uses apache::thrift::CompactSerializer from fbthrift v2025.12.29.00
 * to serialize metadata in CompactProtocol format with magic bytes.
 *
 * Key Features:
 * - CompactProtocol: Space-efficient binary encoding
 * - Magic bytes: {0x82, 0x21} for format detection
 * - Priority: 100 (medium-high, between Legacy 50 and FlatBuffers 120)
 * - Dependencies: Folly + fbthrift + jemalloc
 * - Size: ~100% baseline (smallest format)
 *
 * Architecture:
 * - Domain Model → Thrift Types → CompactProtocol → Bytes
 * - Bytes → CompactProtocol → Thrift Types → Domain Model
 *
 * Magic Bytes:
 * - {0x82, 0x21} = CompactProtocol header
 * - Prepended to serialized data for format detection
 * - Priority 100 ensures detection before Legacy Thrift (priority 50)
 */
class ModernThriftSerializer : public IMetadataSerializer {
public:
  /**
   * Serialize domain metadata to Modern Thrift CompactProtocol
   *
   * @param metadata Pointer to domain::metadata object
   * @return Byte vector with magic bytes + CompactProtocol data
   * @throws std::runtime_error on serialization failure
   *
   * Process:
   * 1. Convert domain::metadata → thrift::metadata (via converter)
   * 2. Serialize using apache::thrift::CompactSerializer
   * 3. Prepend magic bytes {0x82, 0x21}
   */
  std::vector<uint8_t> serialize(const void* metadata) const override;

  /**
   * Deserialize Modern Thrift CompactProtocol to domain metadata
   *
   * @param data Byte vector with magic bytes + CompactProtocol data
   * @return Unique pointer to deserialized domain::metadata
   * @throws std::runtime_error on deserialization failure or invalid magic
   *
   * Process:
   * 1. Verify magic bytes {0x82, 0x21}
   * 2. Deserialize using apache::thrift::CompactSerializer
   * 3. Convert thrift::metadata → domain::metadata (via converter)
   */
  std::unique_ptr<void, void(*)(void*)> deserialize(
      const std::vector<uint8_t>& data) const override;

  /**
   * Get human-readable format name
   * @return "Modern Thrift Compact"
   */
  std::string_view get_format_name() const noexcept override {
    return "Modern Thrift Compact";
  }

  /**
   * Get serialization format identifier
   * @return SerializationFormat::MODERN_THRIFT
   */
  SerializationFormat get_format() const noexcept override {
    return SerializationFormat::MODERN_THRIFT;
  }

  /**
   * Check if this serializer can write metadata
   * @return true (always writable)
   */
  bool can_write() const noexcept override { return true; }

  /**
   * Check if this serializer can read metadata
   * @return true (always readable)
   */
  bool can_read() const noexcept override { return true; }

  /**
   * Get magic bytes for Modern Thrift CompactProtocol
   * @return {0x82, 0x21} - CompactProtocol header bytes
   */
  std::vector<uint8_t> get_magic_bytes() const noexcept override {
    return {MAGIC_BYTES[0], MAGIC_BYTES[1]};
  }

private:
  /**
   * Magic bytes for Modern Thrift CompactProtocol format
   *
   * {0x82, 0x21} = CompactProtocol struct header
   * - 0x82 = field type (struct) + field delta (1)
   * - 0x21 = field type indicator
   *
   * These bytes appear at the start of every CompactProtocol message.
   */
  static constexpr std::array<uint8_t, 2> MAGIC_BYTES = {0x82, 0x21};
};

/**
 * Registration function to ensure static initializer runs
 *
 * Called from init_serializers() to register Modern Thrift
 * serializer with the SerializerRegistry.
 */
void register_modern_thrift_serializer();

} // namespace dwarfs::metadata::serialization

#endif // DWARFS_HAVE_EXPERIMENTAL_THRIFT