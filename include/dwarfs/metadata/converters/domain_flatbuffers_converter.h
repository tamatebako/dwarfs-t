/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \file domain_flatbuffers_converter.h
 *
 * Bidirectional Conversion Between FlatBuffers and Domain Model Metadata
 *
 * This converter provides functions to transform between FlatBuffers
 * metadata structures and domain model classes defined in metadata/domain/.
 * These converters enable the Strategy Pattern implementation where both
 * Thrift and FlatBuffers can interop via the domain model.
 *
 * Design Principles:
 * - Adapter Pattern: Translates between formats
 * - No format knowledge in domain model
 * - Bidirectional conversion support
 * - Handle all metadata versions (v2.0 through v2.5+)
 * - NO PREPROCESSOR GUARDS: CMake controls compilation
 *
 * \author Ribose Inc.
 * \date 2025-12-22
 * \copyright See LICENSE file
 */

#pragma once

#include "dwarfs/metadata/domain/metadata.h"
#include <flatbuffers/flatbuffers.h>
#include <memory>

namespace dwarfs::flatbuffers {
  struct Chunk;
  struct Directory;
  struct InodeData;
  struct DirEntry;
  struct FsOptions;
  struct StringTable;
  struct InodeSizeCache;
  struct HistoryEntry;
  struct Metadata;
}

namespace dwarfs::metadata::converters {

// ============================================================================
// FlatBuffers → Domain Conversions
// ============================================================================

/**
 * Convert FlatBuffers chunk to domain chunk
 *
 * @param fb FlatBuffers chunk structure
 * @return Domain model chunk
 */
domain::chunk from_flatbuffers(const dwarfs::flatbuffers::Chunk& fb);

/**
 * Convert FlatBuffers directory to domain directory
 *
 * @param fb FlatBuffers directory structure
 * @return Domain model directory
 */
domain::directory from_flatbuffers(const dwarfs::flatbuffers::Directory& fb);

/**
 * Convert FlatBuffers inode_data to domain inode_data
 *
 * @param fb FlatBuffers inode_data structure
 * @return Domain model inode_data
 */
domain::inode_data from_flatbuffers(const dwarfs::flatbuffers::InodeData& fb);

/**
 * Convert FlatBuffers dir_entry to domain dir_entry
 *
 * @param fb FlatBuffers dir_entry structure
 * @return Domain model dir_entry
 */
domain::dir_entry from_flatbuffers(const dwarfs::flatbuffers::DirEntry& fb);

/**
 * Convert FlatBuffers fs_options to domain fs_options
 *
 * @param fb FlatBuffers fs_options structure
 * @return Domain model fs_options
 */
domain::fs_options from_flatbuffers(const dwarfs::flatbuffers::FsOptions& fb);

/**
 * Convert FlatBuffers string_table to domain string_table
 *
 * @param fb FlatBuffers string_table structure
 * @return Domain model string_table
 */
domain::string_table from_flatbuffers(const dwarfs::flatbuffers::StringTable& fb);

/**
 * Convert FlatBuffers inode_size_cache to domain inode_size_cache
 *
 * @param fb FlatBuffers inode_size_cache structure
 * @return Domain model inode_size_cache
 */
domain::inode_size_cache from_flatbuffers(const dwarfs::flatbuffers::InodeSizeCache& fb);

/**
 * Convert FlatBuffers history_entry to domain history_entry
 *
 * @param fb FlatBuffers history_entry structure
 * @return Domain model history_entry
 */
domain::history_entry from_flatbuffers(const dwarfs::flatbuffers::HistoryEntry& fb);

/**
 * Convert FlatBuffers metadata to domain metadata (ROOT STRUCTURE)
 *
 * This is the main conversion function that handles all metadata versions.
 *
 * @param fb FlatBuffers metadata structure
 * @return Domain model metadata
 */
domain::metadata from_flatbuffers(const dwarfs::flatbuffers::Metadata& fb);

// ============================================================================
// Domain → FlatBuffers Conversions
// ============================================================================

/**
 * Convert domain chunk to FlatBuffers chunk
 *
 * @param builder FlatBufferBuilder for creating offsets
 * @param c Domain model chunk
 * @return FlatBuffers chunk offset
 */
::flatbuffers::Offset<dwarfs::flatbuffers::Chunk> to_flatbuffers(
    ::flatbuffers::FlatBufferBuilder& builder,
    const domain::chunk& c);

/**
 * Convert domain directory to FlatBuffers directory
 *
 * @param builder FlatBufferBuilder for creating offsets
 * @param d Domain model directory
 * @return FlatBuffers directory offset
 */
::flatbuffers::Offset<dwarfs::flatbuffers::Directory> to_flatbuffers(
    ::flatbuffers::FlatBufferBuilder& builder,
    const domain::directory& d);

/**
 * Convert domain inode_data to FlatBuffers inode_data
 *
 * @param builder FlatBufferBuilder for creating offsets
 * @param i Domain model inode_data
 * @return FlatBuffers inode_data offset
 */
::flatbuffers::Offset<dwarfs::flatbuffers::InodeData> to_flatbuffers(
    ::flatbuffers::FlatBufferBuilder& builder,
    const domain::inode_data& i);

/**
 * Convert domain dir_entry to FlatBuffers dir_entry
 *
 * @param builder FlatBufferBuilder for creating offsets
 * @param e Domain model dir_entry
 * @return FlatBuffers dir_entry offset
 */
::flatbuffers::Offset<dwarfs::flatbuffers::DirEntry> to_flatbuffers(
    ::flatbuffers::FlatBufferBuilder& builder,
    const domain::dir_entry& e);

/**
 * Convert domain fs_options to FlatBuffers fs_options
 *
 * @param builder FlatBufferBuilder for creating offsets
 * @param opts Domain model fs_options
 * @return FlatBuffers fs_options offset
 */
::flatbuffers::Offset<dwarfs::flatbuffers::FsOptions> to_flatbuffers(
    ::flatbuffers::FlatBufferBuilder& builder,
    const domain::fs_options& opts);

/**
 * Convert domain string_table to FlatBuffers string_table
 *
 * @param builder FlatBufferBuilder for creating offsets
 * @param st Domain model string_table
 * @return FlatBuffers string_table offset
 */
::flatbuffers::Offset<dwarfs::flatbuffers::StringTable> to_flatbuffers(
    ::flatbuffers::FlatBufferBuilder& builder,
    const domain::string_table& st);

/**
 * Convert domain inode_size_cache to FlatBuffers inode_size_cache
 *
 * @param builder FlatBufferBuilder for creating offsets
 * @param cache Domain model inode_size_cache
 * @return FlatBuffers inode_size_cache offset
 */
::flatbuffers::Offset<dwarfs::flatbuffers::InodeSizeCache> to_flatbuffers(
    ::flatbuffers::FlatBufferBuilder& builder,
    const domain::inode_size_cache& cache);

/**
 * Convert domain history_entry to FlatBuffers history_entry
 *
 * @param builder FlatBufferBuilder for creating offsets
 * @param e Domain model history_entry
 * @return FlatBuffers history_entry offset
 */
::flatbuffers::Offset<dwarfs::flatbuffers::HistoryEntry> to_flatbuffers(
    ::flatbuffers::FlatBufferBuilder& builder,
    const domain::history_entry& e);

/**
 * Convert domain metadata to FlatBuffers metadata (ROOT STRUCTURE)
 *
 * @param builder FlatBufferBuilder for creating offsets
 * @param m Domain model metadata
 * @return FlatBuffers metadata offset
 */
::flatbuffers::Offset<dwarfs::flatbuffers::Metadata> to_flatbuffers(
    ::flatbuffers::FlatBufferBuilder& builder,
    const domain::metadata& m);

} // namespace dwarfs::metadata::converters