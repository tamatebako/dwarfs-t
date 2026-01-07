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

#ifdef DWARFS_HAVE_THRIFT
#include <dwarfs/metadata/converters/domain_thrift_converter.h>
#include <thrift/lib/cpp2/frozen/FrozenUtil.h>
#include <dwarfs/gen-cpp2/metadata_types.h>
#include <dwarfs/gen-cpp2/metadata_layouts.h>
#include <dwarfs/reader/internal/metadata_types_thrift.h>
#endif

namespace dwarfs::reader::internal {

#if defined(DWARFS_HAVE_THRIFT) && !defined(DWARFS_HAVE_FLATBUFFERS)
namespace {
// Thread-local cache for frozen Thrift metadata
// This allows us to create views from domain model in thrift-only builds
struct thrift_metadata_cache {
  apache::thrift::frozen::Bundled<
      apache::thrift::frozen::Layout<dwarfs::thrift::metadata::metadata>::View> frozen;
  std::unique_ptr<thrift_backend::global_metadata> global;

  thrift_metadata_cache(metadata::domain::metadata const& domain_meta, logger& lgr)
      : frozen(apache::thrift::frozen::freeze(
            metadata::converters::to_thrift(domain_meta)))
      , global(std::make_unique<thrift_backend::global_metadata>(lgr, frozen)) {}
};

// Get or create cached thrift metadata for current domain model
// Uses pointer-based identity to avoid expensive comparisons
thrift_backend::global_metadata const& get_thrift_global(
    metadata::domain::metadata const& domain_meta,
    logger& lgr) {

  thread_local std::unordered_map<void const*, std::shared_ptr<thrift_metadata_cache>> cache;

  void const* key = &domain_meta;
  auto it = cache.find(key);
  if (it == cache.end()) {
    auto new_cache = std::make_shared<thrift_metadata_cache>(domain_meta, lgr);
    it = cache.emplace(key, std::move(new_cache)).first;
  }

  return *it->second->global;
}
} // anonymous namespace
#endif

chunk_range backend_adapter::make_chunk_range(
    metadata::domain::metadata const& domain_meta,
    uint32_t begin,
    uint32_t end) {

#if defined(DWARFS_HAVE_FLATBUFFERS) && !defined(DWARFS_HAVE_THRIFT)
  // FlatBuffers-only: Direct construction with domain model
  // Type alias: chunk_range = domain_chunk_range_impl
  return chunk_range{domain_meta, begin, end};

#elif defined(DWARFS_HAVE_THRIFT) && !defined(DWARFS_HAVE_FLATBUFFERS)
  // Thrift-only: Convert domain model to Thrift frozen types
  // Type alias: chunk_range = thrift_backend::chunk_range
  //
  // Convert domain → mutable Thrift → frozen Thrift
  auto thrift_meta = metadata::converters::to_thrift(domain_meta);
  auto frozen_meta = apache::thrift::frozen::freeze(thrift_meta);

  // Construct chunk_range using frozen metadata view
  // frozen_meta is Bundled<View>, implicitly convertible to View const&
  return thrift_backend::chunk_range{frozen_meta, begin, end};

#else
  // Dual-format: Wrap domain implementation in interface
  // Type alias: chunk_range = chunk_range_wrapper (uses interface)
  auto range_impl = std::make_shared<domain_chunk_range_impl>(
      domain_meta, begin, end);
  return chunk_range{std::static_pointer_cast<chunk_range_interface const>(range_impl)};

#endif
}

#if !defined(DWARFS_HAVE_FLATBUFFERS) || !defined(DWARFS_HAVE_THRIFT)
// Single-format builds: convert from domain types
dir_entry_view backend_adapter::make_dir_entry_view(
    std::shared_ptr<domain_dir_entry_view_impl const> domain_impl) {

#if defined(DWARFS_HAVE_FLATBUFFERS) && !defined(DWARFS_HAVE_THRIFT)
  // FlatBuffers-only: Direct pass-through (domain types are native)
  // Type alias: dir_entry_view wraps domain_dir_entry_view_impl
  return dir_entry_view{domain_impl};

#elif defined(DWARFS_HAVE_THRIFT) && !defined(DWARFS_HAVE_FLATBUFFERS)
  // Thrift-only: Convert via cached thrift global metadata
  // Get domain view data
  uint32_t self_index = domain_impl->self_index();
  uint32_t parent_index = domain_impl->parent_index();

  // Get cached thrift global metadata
  // We use a dummy logger for view construction
  static stream_logger dummy_logger(std::cerr, logger_options{});
  auto const& thrift_global = get_thrift_global(domain_impl->domain_meta(), dummy_logger);

  // Create thrift dir_entry_view_impl
  auto thrift_impl = thrift_backend::dir_entry_view_impl::from_dir_entry_index_shared(
      self_index, parent_index, thrift_global);

  // Wrap in dir_entry_view (public type aliases to thrift_backend::dir_entry_view_impl)
  return dir_entry_view{thrift_impl};

#endif
}

inode_view backend_adapter::make_inode_view(
    std::shared_ptr<domain_inode_view_impl> domain_impl) {

#if defined(DWARFS_HAVE_FLATBUFFERS) && !defined(DWARFS_HAVE_THRIFT)
  // FlatBuffers-only: Direct pass-through
  return inode_view{domain_impl};

#elif defined(DWARFS_HAVE_THRIFT) && !defined(DWARFS_HAVE_FLATBUFFERS)
  // Thrift-only: Convert via cached thrift global metadata
  uint32_t inode_index = domain_impl->inode_index();
  uint32_t inode_num = domain_impl->inode_num();

  // Get cached thrift global metadata
  static stream_logger dummy_logger(std::cerr, logger_options{});
  auto const& thrift_global = get_thrift_global(domain_impl->domain_meta(), dummy_logger);

  // Create thrift inode_view_impl
  auto const& thrift_meta = thrift_global.meta();
  auto thrift_impl = std::make_shared<thrift_backend::inode_view_impl>(
      thrift_meta.inodes()[inode_index], inode_num, thrift_meta);

  // In thrift-only, inode_view expects std::shared_ptr<internal::inode_view_impl const>
  // which is aliased to thrift_backend::inode_view_impl const
  return inode_view{thrift_impl};

#endif
}

directory_view backend_adapter::make_directory_view(
    uint32_t inode,
    domain_global_metadata const& global) {

#if defined(DWARFS_HAVE_FLATBUFFERS) && !defined(DWARFS_HAVE_THRIFT)
  // FlatBuffers-only: Direct construction
  return directory_view{inode, global};

#elif defined(DWARFS_HAVE_THRIFT) && !defined(DWARFS_HAVE_FLATBUFFERS)
  // Thrift-only: Convert via cached thrift global metadata
  static stream_logger dummy_logger(std::cerr, logger_options{});
  auto const& thrift_global = get_thrift_global(global.domain_meta(), dummy_logger);

  // Create directory_view (public type aliases to thrift_backend in thrift-only)
  return directory_view{inode, thrift_global};

#endif
}
#endif

#if defined(DWARFS_HAVE_FLATBUFFERS) && defined(DWARFS_HAVE_THRIFT)
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