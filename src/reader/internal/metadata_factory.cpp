/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \author     Ribose Inc.
 * \copyright  Copyright (c) Ribose Inc.
 *
 * This file is part of dwarfs.
 *
 * SPDX-License-Identifier: MIT
 */

#include <dwarfs/reader/internal/metadata_factory.h>

#include <fmt/format.h>
#include <iostream>

#include <dwarfs/error.h>
#include <dwarfs/logger.h>
#include <dwarfs/metadata/domain/metadata.h>
#include <dwarfs/metadata/serialization/init_serializers.h>
#include <dwarfs/metadata/serialization/serializer_registry.h>
#include <dwarfs/metadata/serialization/serialization_format.h>
#include <dwarfs/metadata/legacy/frozen2_deserializer.h>
#include <dwarfs/metadata/legacy/frozen_schema_serializer.h>

namespace dwarfs::reader::internal {

std::unique_ptr<dwarfs::metadata::domain::metadata>
metadata_factory::load_metadata([[maybe_unused]] logger& lgr, std::span<uint8_t const> data) {
  using namespace dwarfs::metadata::serialization;

  // Initialize serializers (must be called before using registry)
  init_serializers();

  // Get singleton registry
  auto& registry = SerializerRegistry::instance();

  // Convert span to vector for SerializerRegistry interface
  std::vector<uint8_t> data_vec(data.begin(), data.end());

  // Detect format using SerializerRegistry
  auto format = registry.detect_format(data_vec);

  if (!format) {
    DWARFS_THROW(runtime_error, "Unable to detect metadata format");
  }

  // Create serializer for detected format
  auto serializer = registry.create_serializer(*format);

  if (!serializer) {
    DWARFS_THROW(runtime_error,
        fmt::format("Serializer for format '{}' not available",
                    registry.get_format_name(*format)));
  }

  // Deserialize to domain model
  // Use a helper to handle exceptions from serializer
  auto deserialize_with_error_handling = [&]() -> std::unique_ptr<void, void (*)(void*)> {
    try {
      return serializer->deserialize(data_vec);
    } catch (std::exception const& e) {
      DWARFS_THROW(runtime_error,
          fmt::format("Metadata deserialization failed: {}", e.what()));
    }
  };

  auto metadata_ptr = deserialize_with_error_handling();

  if (!metadata_ptr) {
    DWARFS_THROW(runtime_error, "Metadata deserialization failed");
  }

  // Cast the void* to domain::metadata*
  // SerializerRegistry returns void* for type erasure, but we know it's metadata*
  auto* raw_ptr = static_cast<dwarfs::metadata::domain::metadata*>(
      metadata_ptr.release());

  auto domain_meta = std::unique_ptr<dwarfs::metadata::domain::metadata>(raw_ptr);

  // CRITICAL FIX: If directories were packed with delta compression,
  // we need to decompress the first_entry values after deserialization.
  // The writer delta-compresses first_entry when pack_directories is enabled,
  // but the deserializer just reads the compressed values as-is.
  if (domain_meta->options.has_value() && domain_meta->options.value().packed_directories) {
    uint32_t accumulated = 0;
    for (auto& dir : domain_meta->directories) {
      uint32_t delta = dir.first_entry();
      accumulated += delta;
      dir.set_first_entry(accumulated);
    }
  }

  return domain_meta;
}

std::unique_ptr<dwarfs::metadata::domain::metadata>
metadata_factory::load_metadata_legacy(logger& lgr,
                                      std::span<uint8_t const> schema,
                                      std::span<uint8_t const> data) {
  // Suppress unused parameter warning
  (void)lgr;

  try {
    // Parse schema
    auto parsed_schema = dwarfs::metadata::legacy::FrozenSchemaSerializer::deserialize(schema);

    // Deserialize metadata
    auto meta_value = dwarfs::metadata::legacy::Frozen2Deserializer::deserialize(parsed_schema, data);

    // Create domain metadata
    auto domain_meta = std::unique_ptr<dwarfs::metadata::domain::metadata>(
        new dwarfs::metadata::domain::metadata(std::move(meta_value)));

    return domain_meta;
  } catch (std::exception const& e) {
    DWARFS_THROW(runtime_error,
        fmt::format("Failed to load Legacy Thrift metadata: {}", e.what()));
  }
}

} // namespace dwarfs::reader::internal