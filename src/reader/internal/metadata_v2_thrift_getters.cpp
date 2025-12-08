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

// This file contains getter method implementations for metadata_v2_data
// Split from metadata_v2_thrift.cpp for better maintainability

#include <dwarfs/reader/internal/metadata_v2.h>

#include <algorithm>
#include <optional>
#include <string>
#include <vector>
#include <functional>

#include <dwarfs/file_stat.h>
#include <dwarfs/reader/internal/metadata_v2.h>

namespace dwarfs::reader::internal {

// Forward declaration of metadata_v2_data class
// The actual class definition is in the main file
class metadata_v2_data;

// Note: These implementations need to be added to the metadata_v2_data class
// They will be integrated when we split the main file properly

std::optional<std::string> get_block_category_impl(
    metadata_v2_data const& data, size_t block_number) {
  // Implementation to get block category name for a given block number
  // This accesses meta_.block_categories() and meta_.category_names()
  // Placeholder - needs actual implementation with access to meta_
  return std::nullopt;
}

std::vector<std::string> get_all_block_categories_impl(
    metadata_v2_data const& data) {
  // Implementation to get all unique category names
  // Placeholder - needs actual implementation with access to meta_
  return {};
}

std::vector<size_t> get_block_numbers_by_category_impl(
    metadata_v2_data const& data, std::string_view category) {
  // Implementation to find all block numbers for a given category
  // Placeholder - needs actual implementation with access to meta_
  return {};
}

std::vector<file_stat::uid_type> get_all_uids_impl(
    metadata_v2_data const& data) {
  // Implementation to get all unique UIDs from meta_.uids()
  // Placeholder - needs actual implementation with access to meta_
  return {};
}

std::vector<file_stat::gid_type> get_all_gids_impl(
    metadata_v2_data const& data) {
  // Implementation to get all unique GIDs from meta_.gids()
  // Placeholder - needs actual implementation with access to meta_
  return {};
}

} // namespace dwarfs::reader::internal