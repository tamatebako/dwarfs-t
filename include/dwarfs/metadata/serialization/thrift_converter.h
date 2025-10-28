/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \file
 * \brief Converter between Thrift and domain metadata models
 * \author Marcus Holland-Moritz (github@mhxnet.de)
 * \copyright Copyright (c) Marcus Holland-Moritz
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

#include <optional>
#include <vector>

#include <dwarfs/gen-cpp2/metadata_types.h>

#include "../domain/chunk.h"
#include "../domain/dir_entry.h"
#include "../domain/directory.h"
#include "../domain/fs_options.h"
#include "../domain/history_entry.h"
#include "../domain/inode_data.h"
#include "../domain/inode_size_cache.h"
#include "../domain/metadata.h"
#include "../domain/string_table.h"

namespace dwarfs::metadata::serialization {

/**
 * Converter between Thrift and domain metadata models
 *
 * Provides static conversion methods to transform between Apache Thrift
 * generated types and DwarFS domain models. This isolates Thrift-specific
 * code from the rest of the codebase.
 *
 * Design Principles:
 * - **Adapter Pattern**: Converts between incompatible interfaces
 * - **Single Responsibility**: Only handles model conversion
 * - **Stateless**: All methods are static (no instance state)
 * - **Separation of Concerns**: Keeps Thrift types isolated
 *
 * Conversion Strategy:
 * - Field-by-field mapping between Thrift and domain models
 * - Handles optional fields correctly
 * - Preserves all version-specific fields
 * - Supports all Thrift schema versions (v2.0 through v2.5)
 *
 * \example
 * \code
 * // Convert Thrift metadata to domain model
 * thrift::metadata::metadata thrift_meta;
 * // ... deserialize thrift_meta ...
 *
 * auto domain_meta = ThriftConverter::to_domain(thrift_meta);
 * // Now use domain_meta with clean domain model
 * \endcode
 */
class ThriftConverter {
public:
  /**
   * Convert Thrift metadata to domain metadata
   *
   * Main conversion entry point that converts the complete metadata structure.
   *
   * \param thrift_meta Thrift-generated metadata object
   * \return Converted domain metadata object
   *
   * \example
   * \code
   * thrift::metadata::metadata thrift_meta = deserialize_thrift();
   * auto domain_meta = ThriftConverter::to_domain(thrift_meta);
   * std::cout << "Block size: " << domain_meta.block_size << "\n";
   * \endcode
   */
  static domain::metadata to_domain(
      const thrift::metadata::metadata& thrift_meta);

  /**
   * Convert Thrift chunk to domain chunk
   *
   * \param thrift_chunk Thrift chunk object
   * \return Converted domain chunk object
   */
  static domain::chunk to_domain(
      const thrift::metadata::chunk& thrift_chunk);

  /**
   * Convert Thrift directory to domain directory
   *
   * \param thrift_dir Thrift directory object
   * \return Converted domain directory object
   */
  static domain::directory to_domain(
      const thrift::metadata::directory& thrift_dir);

  /**
   * Convert Thrift inode_data to domain inode_data
   *
   * \param thrift_inode Thrift inode_data object
   * \return Converted domain inode_data object
   */
  static domain::inode_data to_domain(
      const thrift::metadata::inode_data& thrift_inode);

  /**
   * Convert Thrift dir_entry to domain dir_entry
   *
   * \param thrift_entry Thrift dir_entry object
   * \return Converted domain dir_entry object
   */
  static domain::dir_entry to_domain(
      const thrift::metadata::dir_entry& thrift_entry);

  /**
   * Convert Thrift fs_options to domain fs_options
   *
   * \param thrift_opts Thrift fs_options object
   * \return Converted domain fs_options object
   */
  static domain::fs_options to_domain(
      const thrift::metadata::fs_options& thrift_opts);

  /**
   * Convert Thrift string_table to domain string_table
   *
   * \param thrift_table Thrift string_table object
   * \return Converted domain string_table object
   */
  static domain::string_table to_domain(
      const thrift::metadata::string_table& thrift_table);

  /**
   * Convert Thrift inode_size_cache to domain inode_size_cache
   *
   * \param thrift_cache Thrift inode_size_cache object
   * \return Converted domain inode_size_cache object
   */
  static domain::inode_size_cache to_domain(
      const thrift::metadata::inode_size_cache& thrift_cache);

  /**
   * Convert Thrift history_entry to domain history_entry
   *
   * \param thrift_entry Thrift history_entry object
   * \return Converted domain history_entry object
   */
  static domain::history_entry to_domain(
      const thrift::metadata::history_entry& thrift_entry);

  /**
   * Convert vector of Thrift objects to vector of domain objects
   *
   * Generic conversion for vectors of convertible types.
   *
   * \tparam DomainT Domain model type
   * \tparam ThriftT Thrift model type
   * \param thrift_vec Vector of Thrift objects
   * \return Vector of converted domain objects
   */
  template <typename DomainT, typename ThriftT>
  static std::vector<DomainT> to_domain_vector(
      const std::vector<ThriftT>& thrift_vec) {
    std::vector<DomainT> result;
    result.reserve(thrift_vec.size());
    for (const auto& item : thrift_vec) {
      result.push_back(to_domain(item));
    }
    return result;
  }

  /**
   * Convert optional Thrift object to optional domain object
   *
   * Generic conversion for optional types.
   *
   * \tparam DomainT Domain model type
   * \tparam ThriftT Thrift model type
   * \param thrift_opt Optional Thrift object
   * \return Optional converted domain object
   */
  template <typename DomainT, typename ThriftT>
  static std::optional<DomainT> to_domain_optional(
      const std::optional<ThriftT>& thrift_opt) {
    if (!thrift_opt.has_value()) {
      return std::nullopt;
    }
    return to_domain(thrift_opt.value());
  }

private:
  // Static-only class, no instances allowed
  ThriftConverter() = delete;
  ~ThriftConverter() = delete;
  ThriftConverter(const ThriftConverter&) = delete;
  ThriftConverter& operator=(const ThriftConverter&) = delete;
  ThriftConverter(ThriftConverter&&) = delete;
  ThriftConverter& operator=(ThriftConverter&&) = delete;
};

} // namespace dwarfs::metadata::serialization