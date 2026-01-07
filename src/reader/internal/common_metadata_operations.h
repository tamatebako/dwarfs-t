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

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include <nlohmann/json.hpp>

#include <dwarfs/metadata/domain/metadata.h>
#include <dwarfs/reader/internal/metadata_v2.h>
#include <dwarfs/reader/metadata_options.h>

namespace dwarfs {

class logger;
class performance_monitor;
struct filesystem_info;
struct vfs_stat;

namespace reader {

struct fsinfo_options;
struct getattr_options;

namespace internal {

/**
 * Domain-based implementation of all filesystem metadata operations.
 *
 * This class implements the metadata_v2::impl interface by working
 * exclusively on the domain model (metadata::domain::metadata).
 * It contains no format-specific code and can be used with any
 * serialization format (Thrift, FlatBuffers, etc.).
 *
 * Architecture:
 * - All operations work on metadata::domain::metadata (domain model)
 * - No access to frozen Thrift or FlatBuffers types
 * - Single implementation for all filesystem operations
 * - Format adapters deserialize to domain model and use this class
 *
 * This eliminates ~7,288 lines of format-specific duplicate code
 * by providing a single, format-agnostic implementation.
 */
class common_metadata_operations : public metadata_v2::impl {
 public:
  /**
   * Construct common operations from domain metadata.
   *
   * @param lgr Logger for diagnostic messages
   * @param domain_meta Domain model (takes ownership via move)
   * @param options Configuration options for filesystem behavior
   * @param inode_offset Offset to add to all inode numbers
   * @param force_consistency_check Force validation even if not requested
   * @param perfmon Performance monitoring (optional)
   */
  common_metadata_operations(
      logger& lgr,
      metadata::domain::metadata domain_meta,
      metadata_options const& options,
      int inode_offset = 0,
      bool force_consistency_check = false,
      std::shared_ptr<performance_monitor const> const& perfmon = nullptr);

  // Disable copy/move to prevent accidental copies of large domain model
  common_metadata_operations(common_metadata_operations const&) = delete;
  common_metadata_operations& operator=(common_metadata_operations const&) = delete;
  common_metadata_operations(common_metadata_operations&&) = delete;
  common_metadata_operations& operator=(common_metadata_operations&&) = delete;

  ~common_metadata_operations() override = default;

  // ========== metadata_v2::impl interface implementation ==========

  // Consistency and metadata info
  void check_consistency() const override;
  size_t size() const override;

  // Tree traversal
  void walk(std::function<void(dir_entry_view)> const& func) const override;
  void walk_data_order(std::function<void(dir_entry_view)> const& func) const override;

  // Directory operations
  dir_entry_view root() const override;
  std::optional<dir_entry_view> find(std::string_view path) const override;
  std::optional<inode_view> find(int inode) const override;
  std::optional<dir_entry_view> find(int inode, std::string_view name) const override;

  // File attribute operations
  file_stat getattr(inode_view iv, std::error_code& ec) const override;
  file_stat getattr(inode_view iv, getattr_options const& opts,
                    std::error_code& ec) const override;

  // Directory operations
  std::optional<directory_view> opendir(inode_view iv) const override;
  std::optional<dir_entry_view> readdir(directory_view dir, size_t offset) const override;
  size_t dirsize(directory_view dir) const override;

  // Access control
  void access(inode_view iv, int mode, file_stat::uid_type uid,
              file_stat::gid_type gid, std::error_code& ec) const override;

  // File operations
  int open(inode_view iv, std::error_code& ec) const override;
  file_off_t seek(uint32_t inode, file_off_t offset, seek_whence whence,
                  std::error_code& ec) const override;
  std::string readlink(inode_view iv, readlink_mode mode,
                       std::error_code& ec) const override;

  // Filesystem stats
  void statvfs(vfs_stat* stbuf) const override;

  // Chunk operations
  chunk_range get_chunks(int inode, std::error_code& ec) const override;
  size_t block_size() const override;

  // Filesystem features
  bool has_symlinks() const override;
  bool has_sparse_files() const override;

  // Metadata queries
  nlohmann::json get_inode_info(inode_view iv, size_t max_chunks) const override;
  std::optional<std::string> get_block_category(size_t block_number) const override;
  std::optional<nlohmann::json> get_block_category_metadata(size_t block_number) const override;
  std::vector<std::string> get_all_block_categories() const override;
  std::vector<file_stat::uid_type> get_all_uids() const override;
  std::vector<file_stat::gid_type> get_all_gids() const override;
  std::vector<size_t> get_block_numbers_by_category(std::string_view category) const override;

  // Serialization and debugging
  void dump(std::ostream& os, fsinfo_options const& opts,
            filesystem_info const* fsinfo,
            std::function<void(std::string const&, uint32_t)> const& icb) const override;
  nlohmann::json info_as_json(fsinfo_options const& opts,
                               filesystem_info const* fsinfo) const override;
  nlohmann::json as_json() const override;
  std::string serialize_as_json(bool simple) const override;

  // Thrift-specific methods (may return nullptr for non-Thrift sources)
  std::unique_ptr<thrift::metadata::metadata> thaw() const override;
  std::unique_ptr<thrift::metadata::metadata> unpack() const override;
  std::unique_ptr<thrift::metadata::fs_options> thaw_fs_options() const override;

 private:
  // Domain model storage
  metadata::domain::metadata const domain_meta_;

  // Configuration
  logger& lgr_;
  metadata_options const options_;
  int const inode_offset_;

  // Cached values computed from domain model
  // (These will be initialized in the constructor)

  // Timestamp helpers
  uint32_t get_time_resolution() const;
  uint32_t get_nsec_multiplier() const;
  uint64_t get_timestamp_base() const;
  bool is_mtime_only() const;

  void fill_timestamps(file_stat& st,
                      metadata::domain::inode_data const& inode) const;

  // Placeholder for future implementation details
  // TODO: Add necessary helper methods and cached data structures
};

} // namespace internal
} // namespace reader
} // namespace dwarfs