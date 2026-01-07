/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \file
 *
 * Forward declarations for metadata types
 *
 * This header provides type aliases that work in all build configurations:
 * - FlatBuffers-only: aliases to flatbuffers_backend types
 * - Thrift-only: aliases to thrift_backend types
 * - Dual-format: aliases to interface types
 *
 * This enables internal headers to use consistent type names regardless
 * of the build configuration.
 */

#pragma once

#include <dwarfs/reader/internal/metadata_view_interface.h>

// Forward declare wrapper for dual-format builds
#if defined(DWARFS_HAVE_FLATBUFFERS) && defined(DWARFS_HAVE_THRIFT)
#include <dwarfs/reader/internal/chunk_range_wrapper.h>
#endif

namespace dwarfs::reader::internal {

// Forward declarations for backend namespaces
namespace flatbuffers_backend {
class chunk_range;
class inode_view_impl;
class dir_entry_view_impl;
class global_metadata;
} // namespace flatbuffers_backend

namespace thrift_backend {
class chunk_range;
class inode_view_impl;
class dir_entry_view_impl;
class global_metadata;
} // namespace thrift_backend

// Forward declarations for domain types
class domain_chunk_range_impl;
class domain_inode_view_impl;
class domain_dir_entry_view_impl;
class domain_global_metadata;

// Type aliases based on build configuration
#if defined(DWARFS_HAVE_FLATBUFFERS) && !defined(DWARFS_HAVE_THRIFT)
// FlatBuffers-only build: use domain types
using chunk_range = domain_chunk_range_impl;
using inode_view_impl = domain_inode_view_impl;
using dir_entry_view_impl = domain_dir_entry_view_impl;
using global_metadata = domain_global_metadata;

#elif defined(DWARFS_HAVE_THRIFT) && !defined(DWARFS_HAVE_FLATBUFFERS)
// Thrift-only build: use concrete Thrift types
using chunk_range = thrift_backend::chunk_range;
using inode_view_impl = thrift_backend::inode_view_impl;
using dir_entry_view_impl = thrift_backend::dir_entry_view_impl;
using global_metadata = thrift_backend::global_metadata;

#else
// Dual-format build: use interface types for polymorphism
// chunk_range uses wrapper for value semantics
using chunk_range = chunk_range_wrapper;
using inode_view_impl = inode_view_interface;
using dir_entry_view_impl = dir_entry_view_interface;
using global_metadata = global_metadata_interface;
#endif

} // namespace dwarfs::reader::internal