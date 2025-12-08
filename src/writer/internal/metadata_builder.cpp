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

// ============================================================================
// STRATEGY PATTERN: Conditional Constructor Implementation
// ============================================================================
//
// This file contains ONLY the metadata_builder public constructor implementations.
// The actual builder logic is in strategy-specific files:
//   - flatbuffers_metadata_builder.cpp (FlatBuffers strategy)
//   - thrift_metadata_builder.cpp (Thrift strategy)
//
// The constructors are conditionally compiled based on available formats.
// This allows Thrift-only builds without FlatBuffers dependencies.

#include <dwarfs/writer/internal/metadata_builder.h>
#include <dwarfs/writer/metadata_options.h>
#include <dwarfs/metadata/domain/metadata.h>
#include <dwarfs/fstypes.h>
#include <dwarfs/logger.h>

// Include strategy headers so compiler sees inheritance
#ifdef DWARFS_HAVE_FLATBUFFERS
#include <dwarfs/writer/internal/flatbuffers_metadata_builder_impl.h>
#endif

#ifdef DWARFS_HAVE_THRIFT
#include <dwarfs/writer/internal/thrift_metadata_builder_impl.h>
#endif

namespace dwarfs::writer::internal {

// ============================================================================
// Constructor Implementations (Conditional Compilation)
// ============================================================================

#ifdef DWARFS_HAVE_FLATBUFFERS
// FlatBuffers strategy constructors

metadata_builder::metadata_builder(logger& lgr, metadata_options const& options)
    : impl_{make_unique_logging_object<impl, flatbuffers_metadata_builder,
                                       logger_policies>(lgr, options)} {}

metadata_builder::metadata_builder(
    logger& lgr,
    metadata::domain::metadata const& md,
    metadata::domain::fs_options const* orig_fs_options,
    filesystem_version const& orig_fs_version,
    metadata_options const& options)
    : impl_{make_unique_logging_object<impl, flatbuffers_metadata_builder,
                                       logger_policies>(
          lgr, md, orig_fs_options, orig_fs_version, options)} {}

metadata_builder::metadata_builder(
    logger& lgr,
    metadata::domain::metadata&& md,
    metadata::domain::fs_options const* orig_fs_options,
    filesystem_version const& orig_fs_version,
    metadata_options const& options)
    : impl_{make_unique_logging_object<impl, flatbuffers_metadata_builder,
                                       logger_policies>(
          lgr, std::move(md), orig_fs_options, orig_fs_version, options)} {}

#elif defined(DWARFS_HAVE_THRIFT)
// Thrift strategy constructors

metadata_builder::metadata_builder(logger& lgr, metadata_options const& options)
    : impl_{make_unique_logging_object<impl, thrift_metadata_builder,
                                       logger_policies>(lgr, options)} {}

metadata_builder::metadata_builder(
    logger& lgr,
    metadata::domain::metadata const& md,
    metadata::domain::fs_options const* orig_fs_options,
    filesystem_version const& orig_fs_version,
    metadata_options const& options)
    : impl_{make_unique_logging_object<impl, thrift_metadata_builder,
                                       logger_policies>(
          lgr, md, orig_fs_options, orig_fs_version, options)} {}

metadata_builder::metadata_builder(
    logger& lgr,
    metadata::domain::metadata&& md,
    metadata::domain::fs_options const* orig_fs_options,
    filesystem_version const& orig_fs_version,
    metadata_options const& options)
    : impl_{make_unique_logging_object<impl, thrift_metadata_builder,
                                       logger_policies>(
          lgr, std::move(md), orig_fs_options, orig_fs_version, options)} {}

#else
#error "No metadata serialization format available - need either DWARFS_HAVE_FLATBUFFERS or DWARFS_HAVE_THRIFT"
#endif

metadata_builder::~metadata_builder() = default;

// block_mapping::map_chunk implementation
std::vector<block_chunk>
block_mapping::map_chunk(size_t offset, size_t size) const {
  std::vector<block_chunk> mapped;

  size_t pos{0};

  for (auto const& chunk : chunks) {
    if (pos + chunk.size > offset) {
      auto mapped_offset = offset - pos;
      auto mapped_size = std::min(size, chunk.size - mapped_offset);
      mapped.push_back(
          {chunk.block, chunk.offset + mapped_offset, mapped_size});
      size -= mapped_size;
      if (size == 0) {
        break;
      }
      offset += mapped_size;
    }

    pos += chunk.size;
  }

  DWARFS_CHECK(size == 0, "failed to map chunk, size mismatch");

  return mapped;
}

} // namespace dwarfs::writer::internal