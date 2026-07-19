// Create new converter header for domain model
/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \file domain_thrift_converter.h
 *
 * Bidirectional Conversion Between Thrift and Domain Model Metadata
 *
 * This converter provides functions to transform between Apache Thrift
 * metadata structures and domain model classes defined in metadata/domain/.
 * These converters enable the Strategy Pattern implementation where both
 * Thrift and FlatBuffers can interop via the domain model.
 *
 * Design Principles:
 * - Adapter Pattern: Translates between formats
 * - No format knowledge in domain model
 * - Bidirectional conversion support
 * - Handle all metadata versions (v2.0 through v2.5+)
 *
 * \author Ribose Inc.
 * \date 2025-11-17
 * \copyright See LICENSE file
 */

#pragma once

#include "dwarfs/metadata/domain/metadata.h"

// Forward declarations to avoid pulling in Thrift headers
namespace dwarfs::thrift::metadata {
class chunk;
class directory;
class inode_data;
class dir_entry;
class fs_options;
class string_table;
class inode_size_cache;
class history_entry;
class metadata;
} // namespace dwarfs::thrift::metadata

namespace dwarfs::metadata::converters {

// ============================================================================
// Thrift → Domain Conversions
// ============================================================================

/**
 * Convert Thrift chunk to domain chunk
 *
 * @param t Thrift chunk structure
 * @return Domain model chunk
 */
domain::chunk from_thrift(const dwarfs::thrift::metadata::chunk& t);

/**
 * Convert Thrift directory to domain directory
 *
 * @param t Thrift directory structure
 * @return Domain model directory
 */
domain::directory from_thrift(const dwarfs::thrift::metadata::directory& t);

/**
 * Convert Thrift inode_data to domain inode_data
 *
 * @param t Thrift inode_data structure
 * @return Domain model inode_data
 */
domain::inode_data from_thrift(const dwarfs::thrift::metadata::inode_data& t);

/**
 * Convert Thrift dir_entry to domain dir_entry
 *
 * @param t Thrift dir_entry structure
 * @return Domain model dir_entry
 */
domain::dir_entry from_thrift(const dwarfs::thrift::metadata::dir_entry& t);

/**
 * Convert Thrift fs_options to domain fs_options
 *
 * @param t Thrift fs_options structure
 * @return Domain model fs_options
 */
domain::fs_options from_thrift(const dwarfs::thrift::metadata::fs_options& t);

/**
 * Convert Thrift string_table to domain string_table
 *
 * @param t Thrift string_table structure
 * @return Domain model string_table
 */
domain::string_table from_thrift(const dwarfs::thrift::metadata::string_table& t);

/**
 * Convert Thrift inode_size_cache to domain inode_size_cache
 *
 * @param t Thrift inode_size_cache structure
 * @return Domain model inode_size_cache
 */
domain::inode_size_cache from_thrift(const dwarfs::thrift::metadata::inode_size_cache& t);

/**
 * Convert Thrift history_entry to domain history_entry
 *
 * @param t Thrift history_entry structure
 * @return Domain model history_entry
 */
domain::history_entry from_thrift(const dwarfs::thrift::metadata::history_entry& t);

/**
 * Convert Thrift metadata to domain metadata (ROOT STRUCTURE)
 *
 * This is the main conversion function that handles all metadata versions.
 *
 * @param t Thrift metadata structure
 * @return Domain model metadata
 */
domain::metadata from_thrift(const dwarfs::thrift::metadata::metadata& t);

// ============================================================================
// Domain → Thrift Conversions
// ============================================================================

/**
 * Convert domain chunk to Thrift chunk
 *
 * @param c Domain model chunk
 * @return Thrift chunk structure
 */
dwarfs::thrift::metadata::chunk to_thrift(const domain::chunk& c);

/**
 * Convert domain directory to Thrift directory
 *
 * @param d Domain model directory
 * @return Thrift directory structure
 */
dwarfs::thrift::metadata::directory to_thrift(const domain::directory& d);

/**
 * Convert domain inode_data to Thrift inode_data
 *
 * @param i Domain model inode_data
 * @return Thrift inode_data structure
 */
dwarfs::thrift::metadata::inode_data to_thrift(const domain::inode_data& i);

/**
 * Convert domain dir_entry to Thrift dir_entry
 *
 * @param e Domain model dir_entry
 * @return Thrift dir_entry structure
 */
dwarfs::thrift::metadata::dir_entry to_thrift(const domain::dir_entry& e);

/**
 * Convert domain fs_options to Thrift fs_options
 *
 * @param opts Domain model fs_options
 * @return Thrift fs_options structure
 */
dwarfs::thrift::metadata::fs_options to_thrift(const domain::fs_options& opts);

/**
 * Convert domain string_table to Thrift string_table
 *
 * @param st Domain model string_table
 * @return Thrift string_table structure
 */
dwarfs::thrift::metadata::string_table to_thrift(const domain::string_table& st);

/**
 * Convert domain inode_size_cache to Thrift inode_size_cache
 *
 * @param cache Domain model inode_size_cache
 * @return Thrift inode_size_cache structure
 */
dwarfs::thrift::metadata::inode_size_cache to_thrift(const domain::inode_size_cache& cache);

/**
 * Convert domain history_entry to Thrift history_entry
 *
 * @param e Domain model history_entry
 * @return Thrift history_entry structure
 */
dwarfs::thrift::metadata::history_entry to_thrift(const domain::history_entry& e);

/**
 * Convert domain metadata to Thrift metadata (ROOT STRUCTURE)
 *
 * @param m Domain model metadata
 * @return Thrift metadata structure
 */
dwarfs::thrift::metadata::metadata to_thrift(const domain::metadata& m);

} // namespace dwarfs::metadata::converters