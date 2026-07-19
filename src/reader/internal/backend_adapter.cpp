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

#include "backend_adapter.h"

#include <dwarfs/reader/internal/domain_metadata_views.h>
#include <dwarfs/error.h>
#include <dwarfs/logger.h>

#ifdef DWARFS_HAVE_EXPERIMENTAL_THRIFT
#include <dwarfs/metadata/converters/domain_thrift_converter.h>
#include <thrift/lib/cpp2/frozen/FrozenUtil.h>
#include <dwarfs/gen-cpp2/metadata_types.h>
#include <dwarfs/gen-cpp2/metadata_layouts.h>
// Note: metadata_types_thrift.h is obsolete - using domain types instead
#endif

namespace dwarfs::reader::internal {

chunk_range backend_adapter::make_chunk_range(
    metadata::domain::metadata const& domain_meta,
    uint32_t begin,
    uint32_t end) {

#if defined(DWARFS_HAVE_FLATBUFFERS) && !defined(DWARFS_HAVE_EXPERIMENTAL_THRIFT)
  // FlatBuffers-only: Direct construction with domain model
  // Type alias: chunk_range = domain_chunk_range_impl
  return chunk_range{domain_meta, begin, end};

#elif defined(DWARFS_HAVE_EXPERIMENTAL_THRIFT) && !defined(DWARFS_HAVE_FLATBUFFERS)
  // Thrift-only: Use domain types directly
  // Type alias: chunk_range = domain_chunk_range_impl
  // Note: thrift_backend types not implemented yet, using domain types
  return chunk_range{domain_meta, begin, end};

#else
  // Dual-format: Wrap domain implementation in interface
  // Type alias: chunk_range = chunk_range_wrapper (uses interface)
  auto range_impl = std::make_shared<domain_chunk_range_impl>(
      domain_meta, begin, end);
  return chunk_range{std::static_pointer_cast<chunk_range_interface const>(range_impl)};

#endif
}

#if !defined(DWARFS_HAVE_FLATBUFFERS) || !defined(DWARFS_HAVE_EXPERIMENTAL_THRIFT)
// Single-format builds: convert from domain types
dir_entry_view backend_adapter::make_dir_entry_view(
    std::shared_ptr<domain_dir_entry_view_impl const> domain_impl) {

#if defined(DWARFS_HAVE_FLATBUFFERS) && !defined(DWARFS_HAVE_EXPERIMENTAL_THRIFT)
  // FlatBuffers-only: Direct pass-through (domain types are native)
  // Type alias: dir_entry_view wraps domain_dir_entry_view_impl
  return dir_entry_view{domain_impl};

#elif defined(DWARFS_HAVE_EXPERIMENTAL_THRIFT) && !defined(DWARFS_HAVE_FLATBUFFERS)
  // Thrift-only: Direct pass-through (domain types are native)
  // Note: thrift_backend types not implemented yet, using domain types directly
  return dir_entry_view{domain_impl};

#endif
}

inode_view backend_adapter::make_inode_view(
    std::shared_ptr<domain_inode_view_impl> domain_impl) {

#if defined(DWARFS_HAVE_FLATBUFFERS) && !defined(DWARFS_HAVE_EXPERIMENTAL_THRIFT)
  // FlatBuffers-only: Direct pass-through
  return inode_view{domain_impl};

#elif defined(DWARFS_HAVE_EXPERIMENTAL_THRIFT) && !defined(DWARFS_HAVE_FLATBUFFERS)
  // Thrift-only: Direct pass-through
  // Note: thrift_backend types not implemented yet, using domain types directly
  return inode_view{domain_impl};

#endif
}

directory_view backend_adapter::make_directory_view(
    uint32_t inode,
    domain_global_metadata const& global) {

#if defined(DWARFS_HAVE_FLATBUFFERS) && !defined(DWARFS_HAVE_EXPERIMENTAL_THRIFT)
  // FlatBuffers-only: Direct construction
  return directory_view{inode, global};

#elif defined(DWARFS_HAVE_EXPERIMENTAL_THRIFT) && !defined(DWARFS_HAVE_FLATBUFFERS)
  // Thrift-only: Direct construction
  // Note: thrift_backend types not implemented yet, using domain types directly
  return directory_view{inode, global};

#endif
}
#endif

#if defined(DWARFS_HAVE_FLATBUFFERS) && defined(DWARFS_HAVE_EXPERIMENTAL_THRIFT)
// Dual-format: pass-through since domain_global_metadata returns interface types
dir_entry_view backend_adapter::make_dir_entry_view(
    std::shared_ptr<dir_entry_view_interface const> interface_impl) {
  return dir_entry_view{interface_impl};
}

inode_view backend_adapter::make_inode_view(
    std::shared_ptr<inode_view_interface const> interface_impl) {
  return inode_view{interface_impl};
}

directory_view backend_adapter::make_directory_view(
    uint32_t inode,
    global_metadata_interface const& global) {
  return directory_view{inode, global};
}
#endif

} // namespace dwarfs::reader::internal