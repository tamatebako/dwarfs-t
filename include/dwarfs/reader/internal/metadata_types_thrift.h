/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \file
 *
 * Thrift backend type aliases
 *
 * This header provides type aliases for Thrift backend implementations.
 * The actual implementation uses the domain types from the modern metadata system.
 *
 * This is a compatibility header to allow builds with Modern Thrift enabled.
 * The real implementation is in the domain metadata types.
 */

#pragma once

// Include the modern Thrift generated headers
#include <dwarfs/gen-cpp2/metadata_modern_types.h>

// Include domain metadata implementation
#include <dwarfs/reader/internal/domain_metadata_impl.h>

namespace dwarfs::reader::internal {

// Thrift backend namespace - uses domain types as implementation
// TODO: Implement true thrift_backend with frozen layout views
namespace thrift_backend {

// Type aliases that forward to domain types
// This allows the code to compile while we transition to the full implementation
using chunk_range = domain_chunk_range_impl;
using inode_view_impl = domain_inode_view_impl;
using dir_entry_view_impl = domain_dir_entry_view_impl;
using global_metadata = domain_global_metadata;

} // namespace thrift_backend

} // namespace dwarfs::reader::internal
