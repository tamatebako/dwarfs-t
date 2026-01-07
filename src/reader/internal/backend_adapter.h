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

#pragma once

#include <dwarfs/metadata/domain/metadata.h>
#include <dwarfs/reader/internal/metadata_types_fwd.h>
#include <dwarfs/reader/metadata_types.h>
#include <memory>

namespace dwarfs::reader::internal {

// Forward declarations for domain implementation types
class domain_dir_entry_view_impl;
class domain_inode_view_impl;
class domain_global_metadata;

/**
 * Backend adapter for creating metadata view types from domain model.
 *
 * This adapter provides helper functions to construct chunk_range, inode_view,
 * dir_entry_view, directory_view, and other metadata view types from the domain
 * model, handling all three build configurations:
 *
 * - FlatBuffers-only: Direct construction with domain model
 * - Thrift-only: Convert domain model to Thrift frozen types
 * - Dual-format: Wrap domain implementation in interface (or pass-through if already interface)
 *
 * This enables common_metadata_operations to work with domain model while
 * supporting backends that expect different types.
 */
class backend_adapter {
 public:
  /**
   * Create a chunk_range from domain metadata.
   *
   * @param domain_meta Domain metadata model
   * @param begin Start chunk index
   * @param end End chunk index (exclusive)
   * @return chunk_range compatible with current build configuration
   */
  static chunk_range make_chunk_range(
      metadata::domain::metadata const& domain_meta,
      uint32_t begin,
      uint32_t end);

#if !defined(DWARFS_HAVE_FLATBUFFERS) || !defined(DWARFS_HAVE_THRIFT)
  /**
   * Create a dir_entry_view from domain implementation (single-format builds).
   *
   * @param domain_impl Domain directory entry view implementation
   * @return dir_entry_view compatible with current build configuration
   */
  static dir_entry_view make_dir_entry_view(
      std::shared_ptr<domain_dir_entry_view_impl const> domain_impl);

  /**
   * Create an inode_view from domain implementation (single-format builds).
   *
   * @param domain_impl Domain inode view implementation
   * @return inode_view compatible with current build configuration
   */
  static inode_view make_inode_view(
      std::shared_ptr<domain_inode_view_impl> domain_impl);

  /**
   * Create a directory_view from domain global metadata (single-format builds).
   *
   * @param inode Inode number
   * @param global Domain global metadata
   * @return directory_view compatible with current build configuration
   */
  static directory_view make_directory_view(
      uint32_t inode,
      domain_global_metadata const& global);
#else
  // Dual-format builds: these are pass-through since domain_global_metadata
  // already returns interface types
  static dir_entry_view make_dir_entry_view(
      std::shared_ptr<dir_entry_view_interface const> interface_impl);

  static inode_view make_inode_view(
      std::shared_ptr<inode_view_interface const> interface_impl);

  static directory_view make_directory_view(
      uint32_t inode,
      global_metadata_interface const& global);
#endif
};

} // namespace dwarfs::reader::internal