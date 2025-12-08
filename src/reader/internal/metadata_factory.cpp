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
#include <dwarfs/reader/internal/metadata_view_interface.h>

#include <dwarfs/error.h>
#include <dwarfs/logger.h>

#ifdef DWARFS_HAVE_FLATBUFFERS
#include <dwarfs/reader/internal/metadata_types_flatbuffers.h>
#include <dwarfs/gen-flatbuffers/metadata.h>
#include <flatbuffers/flatbuffers.h>
#endif

#ifdef DWARFS_HAVE_THRIFT
#include <dwarfs/reader/internal/metadata_types_thrift.h>
#include <thrift/lib/cpp2/frozen/FrozenUtil.h>
#include <dwarfs/gen-cpp2/metadata_layouts.h>
#include <dwarfs/gen-cpp2/metadata_types.h>
#include <dwarfs/gen-cpp2/metadata_types_custom_protocol.h>
#endif

namespace dwarfs::reader::internal {

namespace {

#ifdef DWARFS_HAVE_FLATBUFFERS
// FlatBuffers file identifier for metadata
constexpr char kFlatBuffersIdentifier[] = "DWFS";
constexpr size_t kFlatBuffersIdentifierSize = 4;
#endif

} // anonymous namespace

metadata_format metadata_factory::detect_format(std::span<uint8_t const> data) {
#ifdef DWARFS_HAVE_FLATBUFFERS
  // Check for FlatBuffers magic bytes
  // FlatBuffers stores a file identifier at offset 4-7
  if (data.size() >= 8) {  // Need at least 8 bytes (4 for size prefix + 4 for identifier)
    // FlatBuffers format: [4-byte size][4-byte file identifier][data...]
    // Skip the 4-byte size prefix and check the identifier
    if (std::memcmp(data.data() + 4, kFlatBuffersIdentifier, kFlatBuffersIdentifierSize) == 0) {
      return metadata_format::flatbuffers;
    }
  }
#endif

#ifdef DWARFS_HAVE_THRIFT
  // If no FlatBuffers magic found and Thrift is available, assume Thrift
  // Thrift doesn't have a magic header, so this is a fallback
  return metadata_format::thrift;
#else
  // No Thrift support, default to FlatBuffers
  return metadata_format::flatbuffers;
#endif
}

std::unique_ptr<global_metadata_interface>
metadata_factory::create_global_metadata(logger& lgr, std::span<uint8_t const> data) {
  auto format = detect_format(data);
  return create_global_metadata(lgr, data, format);
}

std::unique_ptr<global_metadata_interface>
metadata_factory::create_global_metadata(logger& lgr, std::span<uint8_t const> data,
                                        metadata_format format) {
  switch (format) {
    case metadata_format::flatbuffers: {
#ifdef DWARFS_HAVE_FLATBUFFERS
      // Verify FlatBuffers data integrity
      ::flatbuffers::Verifier verifier(data.data(), data.size());
      if (!::dwarfs::flatbuffers::VerifyMetadataBuffer(verifier)) {
        DWARFS_THROW(runtime_error, "FlatBuffers metadata verification failed");
      }

      // Get the metadata root
      auto meta = ::dwarfs::flatbuffers::GetMetadata(data.data());
      if (!meta) {
        DWARFS_THROW(runtime_error, "Failed to get FlatBuffers metadata root");
      }

      // Create and return FlatBuffers backend wrapped in unique_ptr to interface
      return std::make_unique<flatbuffers_backend::global_metadata>(lgr, meta);
#else
      DWARFS_THROW(runtime_error, "FlatBuffers support not compiled in");
#endif
    }

    case metadata_format::thrift: {
#ifdef DWARFS_HAVE_THRIFT
      // Map Thrift Frozen2 data using mapFrozen
      auto frozen_meta = ::apache::thrift::frozen::mapFrozen<
          ::dwarfs::thrift::metadata::metadata>(
          ::folly::ByteRange(data.data(), data.size()));
      
      // Bundled implicitly converts to the view type needed by global_metadata
      // Create and return Thrift backend wrapped in unique_ptr to interface
      return std::make_unique<thrift_backend::global_metadata>(lgr, frozen_meta);
#else
      DWARFS_THROW(runtime_error, "Thrift support not compiled in");
#endif
    }

    default:
      DWARFS_THROW(runtime_error, "Unknown metadata format");
  }
}

} // namespace dwarfs::reader::internal