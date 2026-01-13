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

#include <dwarfs/reader/internal/metadata_v2.h>

#include <iostream>
#include <dwarfs/error.h>
#include <dwarfs/logger.h>
#include <dwarfs/reader/metadata_options.h>
#include <dwarfs/reader/internal/domain_metadata_impl.h>
#include <dwarfs/reader/internal/metadata_factory.h>

#ifdef DWARFS_HAVE_THRIFT
#include <dwarfs/gen-cpp2/metadata_types.h>
#endif

namespace dwarfs::reader::internal {

namespace {

} // anonymous namespace

// Implement public constructor with new architecture
metadata_v2::metadata_v2(
    logger& lgr, std::span<uint8_t const> schema,
    std::span<uint8_t const> data, metadata_options const& options,
    int inode_offset, bool force_consistency_check,
    std::shared_ptr<performance_monitor const> const& perfmon) {

  // Use metadata_factory to load domain::metadata
  // For Legacy Thrift format, we need both schema and frozen data
  // Check if data starts with FlatBuffers magic to detect format
  std::unique_ptr<dwarfs::metadata::domain::metadata> domain_meta;

  // FlatBuffers format uses size-prefixed format with file_identifier "DFBF"
  // The first 4 bytes are the size prefix, next 4 bytes are "DFBF"
  bool is_flatbuffers = false;
  if (data.size() >= 8) {
    // Check for FlatBuffers magic after size prefix
    // data[0:4] = size prefix (little endian)
    // data[4:8] should be "DFBF"
    is_flatbuffers = (data[4] == uint8_t('D') && data[5] == uint8_t('F') &&
                      data[6] == uint8_t('B') && data[7] == uint8_t('F'));
  }

  if (is_flatbuffers) {
    // FlatBuffers format: metadata is self-contained with magic bytes
    domain_meta = metadata_factory::load_metadata(lgr, data);
  } else if (!schema.empty()) {
    // Legacy Thrift format: schema is in a separate section
    domain_meta = metadata_factory::load_metadata_legacy(lgr, schema, data);
  } else {
    // Unknown format - try auto-detection
    domain_meta = metadata_factory::load_metadata(lgr, data);
  }

  // Create domain_metadata_impl
  impl_ = std::make_unique<domain_metadata_impl>(
      std::move(domain_meta), options, inode_offset);

  // Perform consistency check if requested
  if (force_consistency_check || options.check_consistency) {
    impl_->check_consistency();
  }
}

chunk_range metadata_v2::get_chunks(int inode, std::error_code& ec) const {
  return impl_->get_chunks(inode, ec);
}

// ========== metadata_v2_utils implementation ==========

metadata_v2_utils::metadata_v2_utils(metadata_v2 const& meta)
  : impl_(*meta.impl_) {}

void metadata_v2_utils::dump(
    std::ostream& os, fsinfo_options const& opts,
    filesystem_info const* fsinfo,
    std::function<void(std::string const&, uint32_t)> const& icb) const {
  impl_.dump(os, opts, fsinfo, icb);
}

nlohmann::json metadata_v2_utils::info_as_json(
    fsinfo_options const& opts, filesystem_info const* fsinfo) const {
  return impl_.info_as_json(opts, fsinfo);
}

nlohmann::json metadata_v2_utils::as_json() const {
  return impl_.as_json();
}

std::string metadata_v2_utils::serialize_as_json(bool simple) const {
  return impl_.serialize_as_json(simple);
}

#ifdef DWARFS_HAVE_THRIFT
std::unique_ptr<thrift::metadata::metadata> metadata_v2_utils::thaw() const {
  return impl_.thaw();
}

std::unique_ptr<thrift::metadata::metadata> metadata_v2_utils::unpack() const {
  return impl_.unpack();
}

std::unique_ptr<thrift::metadata::fs_options>
metadata_v2_utils::thaw_fs_options() const {
  return impl_.thaw_fs_options();
}
#else
std::unique_ptr<thrift::metadata::metadata> metadata_v2_utils::thaw() const {
  DWARFS_THROW(runtime_error, "Thrift support not compiled in");
}

std::unique_ptr<thrift::metadata::metadata> metadata_v2_utils::unpack() const {
  DWARFS_THROW(runtime_error, "Thrift support not compiled in");
}

std::unique_ptr<thrift::metadata::fs_options>
metadata_v2_utils::thaw_fs_options() const {
  DWARFS_THROW(runtime_error, "Thrift support not compiled in");
}
#endif

} // namespace dwarfs::reader::internal