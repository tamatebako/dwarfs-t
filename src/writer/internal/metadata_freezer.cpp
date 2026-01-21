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
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <dwarfs/logger.h>
#include <dwarfs/malloc_byte_buffer.h>
#include <dwarfs/metadata/serialization/serializer_registry.h>
#include <dwarfs/metadata/serialization/init_serializers.h>
#include <dwarfs/metadata/serialization/serialization_facade.h>
#include <dwarfs/metadata/serialization/facade_factory.h>

#include <dwarfs/writer/internal/metadata_freezer.h>

// NEW: Include domain model and converter
#include <dwarfs/metadata/domain/metadata.h>

#ifdef DWARFS_HAVE_EXPERIMENTAL_THRIFT
#include <dwarfs/gen-cpp2/metadata_types.h>
#include <dwarfs/metadata/converters/domain_thrift_converter.h>
#endif

#ifdef DWARFS_HAVE_EXPERIMENTAL_THRIFT
#include <thrift/lib/cpp2/frozen/FrozenUtil.h>
#include <thrift/lib/cpp2/protocol/Serializer.h>
#include <dwarfs/gen-cpp2/metadata_layouts.h>
#include <thrift/lib/thrift/gen-cpp2/frozen_types_custom_protocol.h>
#endif

namespace dwarfs::writer::internal {

namespace {

#ifdef DWARFS_HAVE_EXPERIMENTAL_THRIFT
template <class T>
std::pair<shared_byte_buffer, shared_byte_buffer> freeze_to_buffer(T const& x) {
  using namespace ::apache::thrift::frozen;

  Layout<T> layout;
  size_t content_size = LayoutRoot::layout(x, layout);

  std::string schema;
  serializeRootLayout(layout, schema);

  auto schema_buffer = malloc_byte_buffer::create(schema);

  auto data_buffer = malloc_byte_buffer::create_zeroed(content_size);

  folly::MutableByteRange content_range(data_buffer.data(), data_buffer.size());
  ByteRangeFreezer::freeze(layout, x, content_range);

  data_buffer.resize(data_buffer.size() - content_range.size());
  data_buffer.shrink_to_fit();

  return {schema_buffer.share(), data_buffer.share()};
}
#endif

template <typename LoggerPolicy>
class metadata_freezer_ : public metadata_freezer::impl {
 public:
  explicit metadata_freezer_(logger& lgr,
                            metadata::serialization::SerializationFormat format)
      : LOG_PROXY_INIT(lgr)
      , format_(format) {
    // Initialize serializers if not already done
    metadata::serialization::init_serializers();
  }

  // CHANGED: Accept domain model instead of Thrift
  std::pair<shared_byte_buffer, shared_byte_buffer>
  freeze(metadata::domain::metadata const& data) const override {
    auto ti = LOG_TIMED_VERBOSE;

    using namespace metadata::serialization;

    // For Thrift format: Convert domain → Thrift, then freeze
    if (format_ == SerializationFormat::MODERN_THRIFT) {
#ifdef DWARFS_HAVE_EXPERIMENTAL_THRIFT
      // Convert domain model to Thrift
      auto thrift_data = metadata::converters::to_thrift(data);

      // Freeze Thrift data
      auto rv = freeze_to_buffer(thrift_data);
      ti << "freezing metadata (Thrift Frozen) to " << rv.second.size() << " bytes...";
      return rv;
#else
      throw std::runtime_error(
          "Thrift format not available (build without Thrift support)");
#endif
    }

    // For Legacy Thrift format: Use facade directly with domain model
    if (format_ == SerializationFormat::LEGACY_THRIFT) {
#ifdef DWARFS_HAVE_LEGACY_THRIFT
      // Create facade for Legacy Thrift
      auto facade = FacadeFactory::create(format_);

      // Serialize domain model to Legacy Thrift CompactProtocol
      // Format: [8 bytes] size_prefix + [N bytes] schema + [M bytes] frozen_data
      auto serialized_data = facade->serialize(data);

      // Extract schema from serialized data (skip 8-byte size prefix)
      // Task 5: frozen_data is empty, Task 6 will add metadata encoding
      if (serialized_data.size() < 8) {
        throw std::runtime_error("Legacy Thrift serialization too small (< 8 bytes)");
      }

      // Read size prefix
      uint64_t schema_size;
      std::memcpy(&schema_size, serialized_data.data(), 8);

      if (serialized_data.size() < 8 + schema_size) {
        throw std::runtime_error("Legacy Thrift serialization incomplete");
      }

      // Schema section: Thrift Compact Protocol serialized schema
      std::span<uint8_t const> schema_span(
          serialized_data.data() + 8, schema_size);
      std::vector<uint8_t> schema_data(schema_span.begin(), schema_span.end());
      auto schema_buffer = malloc_byte_buffer::create(schema_data);

      // Data section: Frozen metadata (Task 5: empty, Task 6: will have encoded values)
      std::vector<uint8_t> frozen_data;
      auto data_buffer = malloc_byte_buffer::create(frozen_data);

      ti << "freezing metadata (Legacy Thrift) to schema=" << schema_data.size()
         << " bytes, data=" << frozen_data.size() << " bytes...";

      return {schema_buffer.share(), data_buffer.share()};
#else
      throw std::runtime_error(
          "Legacy Thrift format not available");
#endif
    }

    // For FlatBuffers format: Use facade directly with domain model
    try {
      // Create facade for selected format
      auto facade = FacadeFactory::create(format_);

      // Serialize domain model directly
      auto serialized_data = facade->serialize(data);

      // Modern formats don't use a separate schema
      // Create empty schema buffer (4 zero bytes)
      std::vector<uint8_t> empty_schema = {0, 0, 0, 0};
      auto schema_buffer = malloc_byte_buffer::create(empty_schema);

      // Data contains the full serialized metadata (with magic bytes)
      auto data_buffer = malloc_byte_buffer::create(serialized_data);

      ti << "freezing metadata (" << facade->get_format_name() << ") to "
         << data_buffer.size() << " bytes...";

      return {schema_buffer.share(), data_buffer.share()};

    } catch (const std::exception& e) {
      LOG_ERROR << "Failed to serialize metadata with format "
                << get_format_name(format_) << ": " << e.what();
      throw;
    }
  }

 private:
  LOG_PROXY_DECL(LoggerPolicy);
  metadata::serialization::SerializationFormat format_;
};

} // namespace

metadata_freezer::metadata_freezer(
    logger& lgr, metadata::serialization::SerializationFormat format)
    : impl_{
          make_unique_logging_object<impl, metadata_freezer_, logger_policies>(
              lgr, format)} {}

metadata_freezer::~metadata_freezer() = default;

} // namespace dwarfs::writer::internal
