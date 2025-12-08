/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \author     Marcus Holland-Moritz (github@mhxnet.de)
 * \copyright  Copyright (c) Marcus Holland-Moritz
 *
 * This file is part of dwarfs.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * SPDX-License-Identifier: MIT
 */

// This file only compiles in dual-format builds (both Thrift and FlatBuffers)
// It provides the runtime factory that chooses the correct backend based on format detection

#include <dwarfs/reader/internal/metadata_v2.h>
#include <dwarfs/metadata/serialization/serializer_registry.h>
#include <dwarfs/metadata/serialization/serialization_format.h>
#include <dwarfs/metadata/serialization/init_serializers.h>
#include <dwarfs/logger.h>
#include <dwarfs/error.h>
#include <fmt/format.h>

namespace dwarfs::reader::internal {

// Forward declare factory functions from each backend
// These are implemented in metadata_v2_thrift.cpp and metadata_v2_flatbuffers.cpp
extern metadata_v2 make_metadata_v2_thrift(
    logger& lgr, std::span<uint8_t const> schema,
    std::span<uint8_t const> data, metadata_options const& options,
    int inode_offset, bool force_consistency_check,
    std::shared_ptr<performance_monitor const> const& perfmon);

extern metadata_v2 make_metadata_v2_flatbuffers(
    logger& lgr, std::span<uint8_t const> schema,
    std::span<uint8_t const> data, metadata_options const& options,
    int inode_offset, bool force_consistency_check,
    std::shared_ptr<performance_monitor const> const& perfmon);

// Factory constructor - only compiled in dual-format builds
metadata_v2::metadata_v2(
    logger& lgr, std::span<uint8_t const> schema, std::span<uint8_t const> data,
    metadata_options const& options, int inode_offset,
    bool force_consistency_check,
    std::shared_ptr<performance_monitor const> const& perfmon) {

  // Detect format
  metadata::serialization::init_serializers();
  std::vector<uint8_t> data_vec(data.begin(), std::min(data.begin() + 16, data.end()));
  auto detected = metadata::serialization::SerializerRegistry::instance().detect_format(data_vec);

  if (!detected.has_value()) {
    DWARFS_THROW(runtime_error, "Could not detect metadata format");
  }

  // Dispatch to correct backend based on format
  // In dual-format builds, the Thrift backend handles both formats:
  // - Thrift: uses directly
  // - FlatBuffers: converts to Thrift internally (lines 669-711 in metadata_v2_thrift.cpp)
  if (*detected == metadata::serialization::SerializationFormat::THRIFT_COMPACT ||
      *detected == metadata::serialization::SerializationFormat::FLATBUFFERS) {
    *this = make_metadata_v2_thrift(lgr, schema, data, options,
                                   inode_offset, force_consistency_check, perfmon);
    return;
  }

  DWARFS_THROW(runtime_error,
               fmt::format("Unsupported metadata format: {}",
                          metadata::serialization::get_format_name(*detected)));
}

// Note: get_chunks() is implemented directly by each backend via the virtual
// impl interface. No wrapper method needed in the factory.

} // namespace dwarfs::reader::internal