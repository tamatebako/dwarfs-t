// ... existing code ...
/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \author     Ribose Inc.
 * \copyright  Copyright (c) Ribose Inc.
 *
 * This file is part of dwarfs.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <memory>
#include <functional>
#include <system_error>
#include <span>

#include <dwarfs/reader/internal/metadata_v2.h>
#include <dwarfs/reader/internal/domain_metadata_views.h>
#include <dwarfs/metadata/domain/metadata.h>

namespace dwarfs {

class logger;
struct filesystem_info;

namespace reader {
struct fsinfo_options;
struct metadata_options;
} // namespace reader

namespace thrift {
namespace metadata {
class fs_options;
class metadata;
}
}

namespace reader::internal {

/**
 * Implementation of metadata_v2::impl using domain::metadata
 *
 * Single Responsibility: Provide metadata_v2::impl interface over domain::metadata
 * Dependencies: domain::metadata (domain model), domain_metadata_views (view wrappers)
 *
 * Design Principles:
 * - Clean OOP: Direct implementation, NO adapters or bridges
 * - Open/Closed: Implements stable interface
 * - Dependency Inversion: Depends on domain::metadata abstraction
 * - MECE: All interface methods implemented completely
 *
 * This class wraps domain::metadata and implements all metadata_v2::impl
 * methods by delegating to the domain model and creating appropriate views.
 */
class domain_metadata_impl : public metadata_v2::impl {
 public:
  /**
   * Construct from domain metadata
   *
   * @param meta Domain metadata (takes ownership)
   * @param options Metadata options for initialization
   * @param inode_offset Offset to apply to inode numbers
   */
  explicit domain_metadata_impl(
      std::unique_ptr<metadata::domain::metadata> meta,
      metadata_options const& options,
      int inode_offset);

  // ========== Consistency & Size ==========
  void check_consistency() const override;
  size_t size() const override;

  // ========== Navigation ==========
  void walk(std::function<void(dir_entry_view)> const& func) const override;
  void walk_data_order(std::function<void(dir_entry_view)> const& func) const override;
  dir_entry_view root() const override;
  std::optional<dir_entry_view> find(std::string_view path) const override;
  std::optional<inode_view> find(int inode) const override;
  std::optional<dir_entry_view> find(int inode, std::string_view name) const override;

  // ========== File Attributes ==========
  file_stat getattr(inode_view iv, std::error_code& ec) const override;
  file_stat getattr(inode_view iv, getattr_options const& opts,
                    std::error_code& ec) const override;
  void access(inode_view iv, int mode, file_stat::uid_type uid,
              file_stat::gid_type gid, std::error_code& ec) const override;
  int open(inode_view iv, std::error_code& ec) const override;
  file_off_t seek(uint32_t inode, file_off_t offset, seek_whence whence,
                  std::error_code& ec) const override;

  // ========== Directory Operations ==========
  std::optional<directory_view> opendir(inode_view iv) const override;
  std::optional<dir_entry_view> readdir(directory_view dir, size_t offset) const override;
  size_t dirsize(directory_view dir) const override;

  // ========== Special Files ==========
  std::string readlink(inode_view iv, readlink_mode mode,
                       std::error_code& ec) const override;
  bool has_symlinks() const override;
  bool has_sparse_files() const override;

  // ========== Filesystem Info ==========
  void statvfs(vfs_stat* stbuf) const override;
  size_t block_size() const override;

  // ========== Chunks ==========
  chunk_range get_chunks(int inode, std::error_code& ec) const override;

  // ========== Block Categories ==========
  std::optional<std::string> get_block_category(size_t block_number) const override;
  std::optional<nlohmann::json> get_block_category_metadata(size_t block_number) const override;
  std::vector<std::string> get_all_block_categories() const override;
  std::vector<size_t> get_block_numbers_by_category(std::string_view category) const override;

  // ========== UID/GID ==========
  std::vector<file_stat::uid_type> get_all_uids() const override;
  std::vector<file_stat::gid_type> get_all_gids() const override;

  // ========== JSON/Debug ==========
  nlohmann::json get_inode_info(inode_view iv, size_t max_chunks) const override;
  void dump(std::ostream& os, fsinfo_options const& opts,
            filesystem_info const* fsinfo,
            std::function<void(std::string const&, uint32_t)> const& icb) const override;
  nlohmann::json info_as_json(fsinfo_options const& opts,
                               filesystem_info const* fsinfo) const override;
  nlohmann::json as_json() const override;
  std::string serialize_as_json(bool simple) const override;

#ifdef DWARFS_HAVE_EXPERIMENTAL_THRIFT
  // Thrift export methods (only available when Modern Thrift is compiled in)
  std::unique_ptr<thrift::metadata::metadata> thaw() const;
  std::unique_ptr<thrift::metadata::metadata> unpack() const;
  std::unique_ptr<thrift::metadata::fs_options> thaw_fs_options() const;
#endif

 private:
  // Core data
  std::unique_ptr<metadata::domain::metadata> meta_;
  domain_global_metadata global_;
  int inode_offset_;
  int file_inode_offset_;  // Calculated offset to first regular file inode

  // UID/GID override options from metadata_options
  std::optional<file_stat::uid_type> fs_uid_override_;
  std::optional<file_stat::gid_type> fs_gid_override_;

  // Helper: Get inode data by index
  metadata::domain::inode_data const& get_inode_by_index(uint32_t index) const;

  // Helper: Get inode data by inode number
  metadata::domain::inode_data const& get_inode_by_num(uint32_t inode_num) const;

  // Helper: Create inode view
  inode_view make_inode_view(uint32_t inode_index, uint32_t inode_num) const;

  // Helper: Create directory entry view
  dir_entry_view make_dir_entry_view(uint32_t self_index, uint32_t parent_index) const;

  // Helper: Get file size for inode (legacy compatibility)
  file_off_t get_file_size(uint32_t inode_index) const;

  // Helper: Get file size for inode with both index and num for legacy images
  file_off_t get_file_size(uint32_t inode_index, uint32_t inode_num) const;

  // Helper: Convert file inode number to chunk table index
  // This handles the mapping from file inode (as stored in dir_entries) to
  // the chunk_table index, accounting for legacy writer bugs and shared files
  uint32_t file_inode_to_chunk_index(int inode) const;

  // Helper: Check if inode is directory
  bool is_directory(uint32_t inode_index) const;

  // Helper: Find directory entry by path
  std::optional<dir_entry_view> find_by_path(std::string_view path) const;

  // Helper: Walk directory tree in data order
  void walk_tree_data_order(uint32_t inode,
                             std::function<void(dir_entry_view)> const& func) const;

  // Helper: Convert directory entry to JSON (recursive)
  nlohmann::json entry_to_json(dir_entry_view entry, bool is_root) const;
};

} // namespace reader::internal
} // namespace dwarfs
// ... existing code ...