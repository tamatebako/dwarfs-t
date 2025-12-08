/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \file cpp_thrift_converter.h
 *
 * Bidirectional Conversion Between Thrift and Plain C++ Metadata Structures
 *
 * This file provides conversion functions to transform between Apache Thrift
 * metadata structures and plain C++ equivalents defined in metadata_structures.h.
 * These converters enable backward compatibility with existing Thrift-based
 * filesystem images while allowing new code to use serialization-agnostic
 * plain C++ structures.
 *
 * Conversion Architecture:
 * - Thrift → C++: Extract data from Thrift frozen layouts to plain structs
 * - C++ → Thrift: Populate Thrift structures from plain C++ data
 * - Handle optional fields: Thrift optional ↔ std::optional
 * - Handle collections: Thrift list/map/set ↔ std::vector/map/set
 *
 * Usage Example:
 * \code
 *   // Thrift → C++ conversion
 *   thrift::metadata::chunk tc = ...;
 *   auto cpp_chunk = from_thrift(tc);
 *
 *   // C++ → Thrift conversion
 *   dwarfs::internal::chunk c = ...;
 *   auto tc2 = to_thrift(c);
 * \endcode
 *
 * \author Ribose
 * \date 2025-11-12
 * \copyright See LICENSE file
 */

#pragma once

#include "dwarfs/internal/metadata_structures.h"

#ifdef DWARFS_HAVE_THRIFT

#include <dwarfs/gen-cpp2/metadata_types.h>
#include <dwarfs/gen-cpp2/features_types.h>
#include <dwarfs/gen-cpp2/history_types.h>
#include <dwarfs/gen-cpp2/compression_types.h>

namespace dwarfs::internal {

// ============================================================================
// Thrift → C++ Conversions (from_thrift)
// ============================================================================

/**
 * Convert Thrift chunk to C++ chunk
 *
 * \param t Thrift chunk structure
 * \return Plain C++ chunk structure
 */
chunk from_thrift(const thrift::metadata::chunk& t);

/**
 * Convert Thrift directory to C++ directory
 *
 * \param t Thrift directory structure
 * \return Plain C++ directory structure
 */
directory from_thrift(const thrift::metadata::directory& t);

/**
 * Convert Thrift inode_data to C++ inode_data
 *
 * Handles both required and optional fields, including v2.5+ extensions
 * and deprecated v2.2 fields for backward compatibility.
 *
 * \param t Thrift inode_data structure
 * \return Plain C++ inode_data structure
 */
inode_data from_thrift(const thrift::metadata::inode_data& t);

/**
 * Convert Thrift dir_entry to C++ dir_entry
 *
 * \param t Thrift dir_entry structure
 * \return Plain C++ dir_entry structure
 */
dir_entry from_thrift(const thrift::metadata::dir_entry& t);

/**
 * Convert Thrift fs_options to C++ fs_options
 *
 * Handles optional time resolution and subsecond resolution fields.
 *
 * \param t Thrift fs_options structure
 * \return Plain C++ fs_options structure
 */
fs_options from_thrift(const thrift::metadata::fs_options& t);

/**
 * Convert Thrift string_table to C++ string_table
 *
 * Handles optional FSST symbol table for compressed string storage.
 *
 * \param t Thrift string_table structure
 * \return Plain C++ string_table structure
 */
string_table from_thrift(const thrift::metadata::string_table& t);

/**
 * Convert Thrift inode_size_cache to C++ inode_size_cache
 *
 * Handles optional allocated_size_lookup for sparse files (v2.5+).
 *
 * \param t Thrift inode_size_cache structure
 * \return Plain C++ inode_size_cache structure
 */
inode_size_cache from_thrift(const thrift::metadata::inode_size_cache& t);

/**
 * Convert Thrift history_entry to C++ history_entry
 *
 * Legacy format used in older metadata versions.
 *
 * \param t Thrift history_entry structure
 * \return Plain C++ history_entry structure
 */
history_entry from_thrift(const thrift::metadata::history_entry& t);

/**
 * Convert Thrift dwarfs_version to C++ dwarfs_version
 *
 * Full version format with Git metadata for development builds.
 *
 * \param t Thrift dwarfs_version structure
 * \return Plain C++ dwarfs_version structure
 */
dwarfs_version from_thrift(const thrift::history::dwarfs_version& t);

/**
 * Convert Thrift history_entry_full to C++ history_entry_full
 *
 * Complete filesystem creation history entry.
 *
 * \param t Thrift history_entry structure
 * \return Plain C++ history_entry_full structure
 */
history_entry_full from_thrift(const thrift::history::history_entry& t);

/**
 * Convert Thrift history to C++ history
 *
 * \param t Thrift history structure
 * \return Plain C++ history structure
 */
history from_thrift(const thrift::history::history& t);

/**
 * Convert Thrift flac_block_header to C++ flac_block_header
 *
 * \param t Thrift flac_block_header structure
 * \return Plain C++ flac_block_header structure
 */
flac_block_header from_thrift(const thrift::compression::flac_block_header& t);

/**
 * Convert Thrift ricepp_block_header to C++ ricepp_block_header
 *
 * \param t Thrift ricepp_block_header structure
 * \return Plain C++ ricepp_block_header structure
 */
ricepp_block_header from_thrift(const thrift::compression::ricepp_block_header& t);

/**
 * Convert Thrift metadata to C++ metadata (ROOT STRUCTURE)
 *
 * This is the main conversion function that handles all metadata versions
 * (v2.0 through v2.5+) with proper handling of optional fields.
 *
 * Conversion Process:
 * 1. Copy core collections (chunks, directories, inodes, etc.)
 * 2. Copy basic scalar fields
 * 3. Convert optional v2.1+ fields
 * 4. Convert optional v2.3+ fields (dir_entries, compact_names, etc.)
 * 5. Convert optional v2.5+ fields (features, categories, caches, etc.)
 *
 * \param t Thrift metadata structure
 * \return Plain C++ metadata structure
 */
metadata from_thrift(const thrift::metadata::metadata& t);

// ============================================================================
// C++ → Thrift Conversions (to_thrift)
// ============================================================================

/**
 * Convert C++ chunk to Thrift chunk
 *
 * \param c Plain C++ chunk structure
 * \return Thrift chunk structure
 */
thrift::metadata::chunk to_thrift(const chunk& c);

/**
 * Convert C++ directory to Thrift directory
 *
 * \param d Plain C++ directory structure
 * \return Thrift directory structure
 */
thrift::metadata::directory to_thrift(const directory& d);

/**
 * Convert C++ inode_data to Thrift inode_data
 *
 * Handles both required and optional fields, only populating Thrift
 * optional fields when corresponding C++ values are non-default.
 *
 * \param i Plain C++ inode_data structure
 * \return Thrift inode_data structure
 */
thrift::metadata::inode_data to_thrift(const inode_data& i);

/**
 * Convert C++ dir_entry to Thrift dir_entry
 *
 * \param e Plain C++ dir_entry structure
 * \return Thrift dir_entry structure
 */
thrift::metadata::dir_entry to_thrift(const dir_entry& e);

/**
 * Convert C++ fs_options to Thrift fs_options
 *
 * \param opts Plain C++ fs_options structure
 * \return Thrift fs_options structure
 */
thrift::metadata::fs_options to_thrift(const fs_options& opts);

/**
 * Convert C++ string_table to Thrift string_table
 *
 * \param st Plain C++ string_table structure
 * \return Thrift string_table structure
 */
thrift::metadata::string_table to_thrift(const string_table& st);

/**
 * Convert C++ inode_size_cache to Thrift inode_size_cache
 *
 * \param cache Plain C++ inode_size_cache structure
 * \return Thrift inode_size_cache structure
 */
thrift::metadata::inode_size_cache to_thrift(const inode_size_cache& cache);

/**
 * Convert C++ history_entry to Thrift history_entry
 *
 * \param e Plain C++ history_entry structure
 * \return Thrift history_entry structure
 */
thrift::metadata::history_entry to_thrift(const history_entry& e);

/**
 * Convert C++ dwarfs_version to Thrift dwarfs_version
 *
 * \param v Plain C++ dwarfs_version structure
 * \return Thrift dwarfs_version structure
 */
thrift::history::dwarfs_version to_thrift(const dwarfs_version& v);

/**
 * Convert C++ history_entry_full to Thrift history_entry
 *
 * \param e Plain C++ history_entry_full structure
 * \return Thrift history_entry structure
 */
thrift::history::history_entry to_thrift(const history_entry_full& e);

/**
 * Convert C++ history to Thrift history
 *
 * \param h Plain C++ history structure
 * \return Thrift history structure
 */
thrift::history::history to_thrift(const history& h);

/**
 * Convert C++ flac_block_header to Thrift flac_block_header
 *
 * \param h Plain C++ flac_block_header structure
 * \return Thrift flac_block_header structure
 */
thrift::compression::flac_block_header to_thrift(const flac_block_header& h);

/**
 * Convert C++ ricepp_block_header to Thrift ricepp_block_header
 *
 * \param h Plain C++ ricepp_block_header structure
 * \return Thrift ricepp_block_header structure
 */
thrift::compression::ricepp_block_header to_thrift(const ricepp_block_header& h);

/**
 * Convert C++ metadata to Thrift metadata (ROOT STRUCTURE)
 *
 * This is the main conversion function that handles all metadata versions
 * with proper handling of optional fields.
 *
 * \param m Plain C++ metadata structure
 * \return Thrift metadata structure
 */
thrift::metadata::metadata to_thrift(const metadata& m);

} // namespace dwarfs::internal

#endif // DWARFS_HAVE_THRIFT