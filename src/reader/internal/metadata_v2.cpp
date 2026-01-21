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

namespace dwarfs::reader::internal {

namespace {

// Helper to check if a span is all zeros
bool is_all_zeros(std::span<uint8_t const> data) {
  return std::all_of(data.begin(), data.end(), [](uint8_t b) { return b == 0; });
}

} // anonymous namespace

// Implement public constructor with new architecture
metadata_v2::metadata_v2(
    logger& lgr, std::span<uint8_t const> schema,
    std::span<uint8_t const> data, metadata_options const& options,
    int inode_offset, bool force_consistency_check,
    std::shared_ptr<performance_monitor const> const& perfmon) {

  // Use metadata_factory to load domain::metadata
  // The data parameter is the decompressed metadata section data
  // The schema parameter is the decompressed schema section data (may be empty)
  std::unique_ptr<dwarfs::metadata::domain::metadata> domain_meta;

  // Check for FlatBuffers format
  // FlatBuffers uses size-prefixed format: 4 bytes size (little endian) + "DFBF" magic
  bool is_flatbuffers = false;
  if (data.size() >= 8) {
    is_flatbuffers = (data[4] == uint8_t('D') &&
                      data[5] == uint8_t('F') &&
                      data[6] == uint8_t('B') &&
                      data[7] == uint8_t('F'));
  }

  if (is_flatbuffers) {
    // FlatBuffers format: use SerializerRegistry
    domain_meta = metadata_factory::load_metadata(lgr, data);
  } else if (!schema.empty() && !is_all_zeros(schema)) {
    // Legacy Thrift format: schema is in a separate section
    // Check that schema is not all zeros (which indicates an empty/invalid schema)
    domain_meta = metadata_factory::load_metadata_legacy(lgr, schema, data);
  } else {
    // Unknown format - try auto-detection with full data
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

} // namespace dwarfs::reader::internal