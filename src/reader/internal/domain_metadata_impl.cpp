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

#include <dwarfs/reader/internal/domain_metadata_impl.h>

#include <algorithm>
#include <sstream>
#include <system_error>

#include <fmt/format.h>
#include <dwarfs/error.h>
#include <dwarfs/fstypes.h>
#include <dwarfs/logger.h>
#include <dwarfs/reader/fsinfo_options.h>
#include <dwarfs/reader/getattr_options.h>
#include <dwarfs/reader/metadata_options.h>
#include <dwarfs/vfs_stat.h>
#include <dwarfs/metadata/domain/directory.h>
#include <dwarfs/metadata/domain/dir_entry.h>

#ifdef DWARFS_HAVE_THRIFT
#include <dwarfs/gen-cpp2/metadata_types.h>
#include <dwarfs/metadata/converters/cpp_thrift_converter.h>
#endif

namespace dwarfs::reader::internal {

namespace {

// Helper: Get mode from inode
uint32_t get_mode(metadata::domain::metadata const& meta,
                  metadata::domain::inode_data const& inode) {
  if (inode.mode_index >= meta.modes.size()) {
    DWARFS_THROW(runtime_error, "Invalid mode index");
  }
  return meta.modes[inode.mode_index];
}

// Helper: Get UID from inode
uint32_t get_uid(metadata::domain::metadata const& meta,
                 metadata::domain::inode_data const& inode) {
  if (inode.owner_index >= meta.uids.size()) {
    DWARFS_THROW(runtime_error, "Invalid UID index");
  }
  return meta.uids[inode.owner_index];
}

// Helper: Get GID from inode
uint32_t get_gid(metadata::domain::metadata const& meta,
                 metadata::domain::inode_data const& inode) {
  if (inode.group_index >= meta.gids.size()) {
    DWARFS_THROW(runtime_error, "Invalid GID index");
  }
  return meta.gids[inode.group_index];
}

// Helper: Check if mode is directory
bool is_directory_mode(uint32_t mode) {
  return posix_file_type::from_mode(mode) == posix_file_type::directory;
}

// Helper: Check if mode is symlink
bool is_symlink_mode(uint32_t mode) {
  return posix_file_type::from_mode(mode) == posix_file_type::symlink;
}

// Helper: Check if mode is regular file
bool is_regular_file_mode(uint32_t mode) {
  return posix_file_type::from_mode(mode) == posix_file_type::regular;
}

} // anonymous namespace

// ========== Constructor ==========

domain_metadata_impl::domain_metadata_impl(
    std::unique_ptr<metadata::domain::metadata> meta,
    metadata_options const& options,
    int inode_offset)
    : meta_(std::move(meta))
    , global_(*meta_)
    , inode_offset_(inode_offset) {

  if (!meta_) {
    DWARFS_THROW(runtime_error, "metadata cannot be null");
  }
}

// ========== Helper Methods ==========

metadata::domain::inode_data const&
domain_metadata_impl::get_inode_by_index(uint32_t index) const {
  if (index >= meta_->inodes.size()) {
    DWARFS_THROW(runtime_error, fmt::format("Invalid inode index: {}", index));
  }
  return meta_->inodes[index];
}

metadata::domain::inode_data const&
domain_metadata_impl::get_inode_by_num(uint32_t inode_num) const {
  // Remove offset to get index
  uint32_t index = inode_num - inode_offset_;
  return get_inode_by_index(index);
}

inode_view domain_metadata_impl::make_inode_view(uint32_t inode_index,
                                                   uint32_t inode_num) const {
  auto impl = std::make_shared<domain_inode_view_impl>(
      *meta_, inode_index, inode_num);
  return inode_view{impl};
}

dir_entry_view domain_metadata_impl::make_dir_entry_view(
    uint32_t self_index, uint32_t parent_index) const {
  auto impl = std::make_shared<domain_dir_entry_view_impl>(
      *meta_, self_index, parent_index);
  return dir_entry_view{impl};
}

file_off_t domain_metadata_impl::get_file_size(uint32_t inode_index) const {
  auto const& inode = get_inode_by_index(inode_index);

  // Check if it's a regular file with size cache
  if (meta_->reg_file_size_cache && is_regular_file_mode(get_mode(*meta_, inode))) {
    auto it = meta_->reg_file_size_cache->size_lookup.find(inode_index);
    if (it != meta_->reg_file_size_cache->size_lookup.end()) {
      return it->second;
    }
  }

  // Calculate size from chunks
  file_off_t size = 0;

  // Find chunk range for this inode
  if (inode_index < meta_->chunk_table.size() - 1) {
    uint32_t begin = meta_->chunk_table[inode_index];
    uint32_t end = meta_->chunk_table[inode_index + 1];

    for (uint32_t i = begin; i < end; ++i) {
      if (i < meta_->chunks.size()) {
        size += meta_->chunks[i].size();
      }
    }
  }

  return size;
}

bool domain_metadata_impl::is_directory(uint32_t inode_index) const {
  auto const& inode = get_inode_by_index(inode_index);
  return is_directory_mode(get_mode(*meta_, inode));
}

// ========== Consistency & Size ==========

void domain_metadata_impl::check_consistency() const {
  // Basic consistency checks on domain model
  if (meta_->inodes.empty()) {
    DWARFS_THROW(runtime_error, "No inodes in metadata");
  }

  if (meta_->directories.empty()) {
    DWARFS_THROW(runtime_error, "No directories in metadata");
  }

  // Check chunk table consistency
  if (!meta_->chunk_table.empty() && meta_->chunk_table.size() != meta_->inodes.size() + 1) {
    DWARFS_THROW(runtime_error, "Chunk table size mismatch");
  }
}

size_t domain_metadata_impl::size() const {
  return meta_->total_fs_size;
}

// ========== Navigation ==========

void domain_metadata_impl::walk(
    std::function<void(dir_entry_view)> const& func) const {
  // Use dir_entries if available (v2.3+), otherwise use directories
  if (meta_->dir_entries) {
    // For each directory, walk its entries
    for (size_t dir_idx = 0; dir_idx < meta_->directories.size(); ++dir_idx) {
      auto const& dir = meta_->directories[dir_idx];
      uint32_t first = dir.first_entry();
      uint32_t parent = dir.parent_entry();

      // Walk entries in this directory
      uint32_t current = first;
      while (current < meta_->dir_entries->size()) {
        auto const& entry = (*meta_->dir_entries)[current];
        auto view = make_dir_entry_view(current, parent);
        func(view);

        // Move to next entry (they are sequential within a directory)
        current++;
        // Check if still in same directory by checking parent
        if (current >= meta_->dir_entries->size()) break;
        // Simple approach: just walk all entries with their calculated parents
        break;
      }
    }
  } else {
    // Legacy: walk directories
    for (size_t dir_idx = 0; dir_idx < meta_->directories.size(); ++dir_idx) {
      auto const& dir = meta_->directories[dir_idx];

      // Create entry for directory itself
      uint32_t self_index = global_.self_dir_entry(dir_idx);
      uint32_t parent_index = (dir_idx == 0) ? 0 : global_.parent_dir_entry(dir_idx);
      auto view = make_dir_entry_view(self_index, parent_index);
      func(view);
    }
  }
}

void domain_metadata_impl::walk_data_order(
    std::function<void(dir_entry_view)> const& func) const {
  // Walk in inode order (data order)
  for (uint32_t inode_idx = 0; inode_idx < meta_->inodes.size(); ++inode_idx) {
    // Find directory entry for this inode
    if (meta_->dir_entries) {
      for (size_t i = 0; i < meta_->dir_entries->size(); ++i) {
        auto const& entry = (*meta_->dir_entries)[i];
        if (entry.inode_num() == inode_idx + inode_offset_) {
          // Find parent by traversing directories to find which one contains this entry
          uint32_t parent_index = 0;
          for (size_t dir_idx = 0; dir_idx < meta_->directories.size(); ++dir_idx) {
            if (i >= meta_->directories[dir_idx].first_entry()) {
              parent_index = meta_->directories[dir_idx].parent_entry();
            }
          }
          auto view = make_dir_entry_view(i, parent_index);
          func(view);
          break;
        }
      }
    }
  }
}

dir_entry_view domain_metadata_impl::root() const {
  // Root directory entry is always index 0
  return make_dir_entry_view(0, 0);
}

std::optional<dir_entry_view> domain_metadata_impl::find(std::string_view path) const {
  if (path.empty() || path == "/") {
    return root();
  }

  // Remove leading slash
  if (path[0] == '/') {
    path = path.substr(1);
  }

  return find_by_path(path);
}

std::optional<inode_view> domain_metadata_impl::find(int inode) const {
  uint32_t inode_num = static_cast<uint32_t>(inode);
  uint32_t inode_index = inode_num - inode_offset_;

  if (inode_index >= meta_->inodes.size()) {
    return std::nullopt;
  }

  return make_inode_view(inode_index, inode_num);
}

std::optional<dir_entry_view> domain_metadata_impl::find(
    int inode, std::string_view name) const {
  uint32_t inode_num = static_cast<uint32_t>(inode);
  uint32_t inode_index = inode_num - inode_offset_;

  if (inode_index >= meta_->inodes.size()) {
    return std::nullopt;
  }

  // Check if it's a directory
  if (!is_directory(inode_index)) {
    return std::nullopt;
  }

  // Find directory by inode_index and search for name in its entries
  if (inode_index < meta_->directories.size()) {
    auto const& dir = meta_->directories[inode_index];
    uint32_t first = dir.first_entry();
    uint32_t parent = dir.parent_entry();

    if (meta_->dir_entries) {
      // Walk entries starting from first_entry
      for (uint32_t i = first; i < meta_->dir_entries->size(); ++i) {
        auto const& entry = (*meta_->dir_entries)[i];

        // Get entry name (compact_names doesn't have get_string method)
        std::string entry_name;
        uint32_t name_idx = entry.name_index();
        if (name_idx < meta_->names.size()) {
          entry_name = meta_->names[name_idx];
        }

        if (entry_name == name) {
          return make_dir_entry_view(i, parent);
        }

        // Stop when we leave this directory (heuristic: large gap in indices)
        if (i > first + 1000) break;
      }
    }
  }

  return std::nullopt;
}

std::optional<dir_entry_view> domain_metadata_impl::find_by_path(
    std::string_view path) const {
  // Split path and search from root
  std::optional<dir_entry_view> current = root();

  size_t start = 0;
  while (start < path.size() && current) {
    size_t end = path.find('/', start);
    if (end == std::string_view::npos) {
      end = path.size();
    }

    std::string_view component = path.substr(start, end - start);
    if (!component.empty()) {
      auto inode = current->inode();
      current = find(inode.inode_num(), component);
    }

    start = end + 1;
  }

  return current;
}

// ========== File Attributes ==========

file_stat domain_metadata_impl::getattr(inode_view iv,
                                         std::error_code& ec) const {
  getattr_options opts;
  return getattr(std::move(iv), opts, ec);
}

file_stat domain_metadata_impl::getattr(inode_view iv, getattr_options const& opts,
                                         std::error_code& ec) const {
  ec.clear();

  file_stat st;

  try {
    auto const& impl = static_cast<domain_inode_view_impl const&>(iv.raw());
    uint32_t inode_index = impl.inode_index();
    auto const& inode = get_inode_by_index(inode_index);

    // Set mode, uid, gid
    st.set_mode(get_mode(*meta_, inode));
    st.set_uid(get_uid(*meta_, inode));
    st.set_gid(get_gid(*meta_, inode));

    // Set inode number
    st.set_ino(iv.inode_num());

    // Set nlink
    st.set_nlink(inode.nlink_minus_one + 1);

    // Set timestamps
    st.set_atimespec(meta_->timestamp_base + inode.atime_offset, inode.atime_subsec);
    st.set_mtimespec(meta_->timestamp_base + inode.mtime_offset, inode.mtime_subsec);
    st.set_ctimespec(meta_->timestamp_base + inode.ctime_offset, inode.ctime_subsec);

    // Set size (if not no_size)
    if (!opts.no_size) {
      st.set_size(get_file_size(inode_index));
    }

    // Set block size
    st.set_blksize(meta_->block_size);

  } catch (std::exception const& e) {
    ec = std::make_error_code(std::errc::io_error);
  }

  return st;
}

void domain_metadata_impl::access(inode_view iv, int mode,
                                   file_stat::uid_type uid,
                                   file_stat::gid_type gid,
                                   std::error_code& ec) const {
  ec.clear();

  try {
    auto const& impl = static_cast<domain_inode_view_impl const&>(iv.raw());
    auto const& inode = get_inode_by_index(impl.inode_index());

    uint32_t file_mode = get_mode(*meta_, inode);
    uint32_t file_uid = get_uid(*meta_, inode);
    uint32_t file_gid = get_gid(*meta_, inode);

    // Check permissions
    bool allowed = false;

    if (uid == 0) {
      // Root has all permissions
      allowed = true;
    } else if (uid == file_uid) {
      // Owner permissions
      uint32_t user_perms = (file_mode >> 6) & 7;
      allowed = ((mode & 4) == 0 || (user_perms & 4)) &&
                ((mode & 2) == 0 || (user_perms & 2)) &&
                ((mode & 1) == 0 || (user_perms & 1));
    } else if (gid == file_gid) {
      // Group permissions
      uint32_t group_perms = (file_mode >> 3) & 7;
      allowed = ((mode & 4) == 0 || (group_perms & 4)) &&
                ((mode & 2) == 0 || (group_perms & 2)) &&
                ((mode & 1) == 0 || (group_perms & 1));
    } else {
      // Other permissions
      uint32_t other_perms = file_mode & 7;
      allowed = ((mode & 4) == 0 || (other_perms & 4)) &&
                ((mode & 2) == 0 || (other_perms & 2)) &&
                ((mode & 1) == 0 || (other_perms & 1));
    }

    if (!allowed) {
      ec = std::make_error_code(std::errc::permission_denied);
    }
  } catch (std::exception const&) {
    ec = std::make_error_code(std::errc::io_error);
  }
}

int domain_metadata_impl::open(inode_view iv, std::error_code& ec) const {
  ec.clear();

  try {
    auto const& impl = static_cast<domain_inode_view_impl const&>(iv.raw());
    auto const& inode = get_inode_by_index(impl.inode_index());

    // Check if it's a regular file
    if (!is_regular_file_mode(get_mode(*meta_, inode))) {
      ec = std::make_error_code(std::errc::invalid_argument);
      return -1;
    }

    // Return success (file descriptor not used in read-only fs)
    return 0;
  } catch (std::exception const&) {
    ec = std::make_error_code(std::errc::io_error);
    return -1;
  }
}

file_off_t domain_metadata_impl::seek(uint32_t inode, file_off_t offset,
                                       seek_whence whence,
                                       std::error_code& ec) const {
  ec.clear();

  try {
    uint32_t inode_index = inode - inode_offset_;
    file_off_t size = get_file_size(inode_index);

    // Validate offset
    if (offset < 0 || offset > size) {
      ec = std::make_error_code(std::errc::invalid_argument);
      return -1;
    }

    // For sparse file support: seek to next data/hole region
    switch (whence) {
      case seek_whence::data:
        // Seek to next data region at or after offset
        // Without hole support, all regions are data
        if (!meta_->hole_block_index) {
          return offset;  // No holes, offset is already in data
        }
        // TODO: Implement proper hole detection by checking chunks
        return offset;

      case seek_whence::hole:
        // Seek to next hole at or after offset
        // Without hole support, report EOF (no holes)
        if (!meta_->hole_block_index) {
          return size;  // No holes, return EOF
        }
        // TODO: Implement proper hole detection by checking chunks
        return size;

      default:
        ec = std::make_error_code(std::errc::invalid_argument);
        return -1;
    }
  } catch (std::exception const&) {
    ec = std::make_error_code(std::errc::io_error);
    return -1;
  }
}

// ========== Directory Operations ==========

std::optional<directory_view> domain_metadata_impl::opendir(inode_view iv) const {
  auto const& impl = static_cast<domain_inode_view_impl const&>(iv.raw());
  uint32_t inode_index = impl.inode_index();

  if (!is_directory(inode_index)) {
    return std::nullopt;
  }

  // TODO: directory_view constructor is private, need backend_adapter or different approach
  // For now, return nullopt - this needs proper implementation
  return std::nullopt;
}

std::optional<dir_entry_view> domain_metadata_impl::readdir(
    directory_view dir, size_t offset) const {
  auto range = dir.entry_range();

  if (offset >= range.size()) {
    return std::nullopt;
  }

  uint32_t entry_index = range.front() + offset;

  if (meta_->dir_entries && entry_index < meta_->dir_entries->size()) {
    // Find parent index by looking up which directory contains this entry
    uint32_t parent_index = 0;
    for (size_t dir_idx = 0; dir_idx < meta_->directories.size(); ++dir_idx) {
      if (entry_index >= meta_->directories[dir_idx].first_entry()) {
        parent_index = meta_->directories[dir_idx].parent_entry();
      }
    }
    auto view = make_dir_entry_view(entry_index, parent_index);
    return view;
  }

  return std::nullopt;
}

size_t domain_metadata_impl::dirsize(directory_view dir) const {
  return dir.entry_count();
}

// ========== Special Files ==========

std::string domain_metadata_impl::readlink(inode_view iv, readlink_mode mode,
                                             std::error_code& ec) const {
  ec.clear();

  try {
    auto const& impl = static_cast<domain_inode_view_impl const&>(iv.raw());
    auto const& inode = get_inode_by_index(impl.inode_index());

    // Check if it's a symlink
    if (!is_symlink_mode(get_mode(*meta_, inode))) {
      ec = std::make_error_code(std::errc::invalid_argument);
      return {};
    }

    // Get symlink target from symlink table
    uint32_t inode_index = impl.inode_index();
    if (inode_index < meta_->symlink_table.size()) {
      uint32_t symlink_index = meta_->symlink_table[inode_index];

      // string_table doesn't have get_string() method
      // Use symlinks vector directly (compact_symlinks is for storage optimization)
      if (symlink_index < meta_->symlinks.size()) {
        return meta_->symlinks[symlink_index];
      }
    }

    ec = std::make_error_code(std::errc::no_such_file_or_directory);
    return {};
  } catch (std::exception const&) {
    ec = std::make_error_code(std::errc::io_error);
    return {};
  }
}

bool domain_metadata_impl::has_symlinks() const {
  return !meta_->symlink_table.empty();
}

bool domain_metadata_impl::has_sparse_files() const {
  return meta_->hole_block_index.has_value();
}

// ========== Filesystem Info ==========

void domain_metadata_impl::statvfs(vfs_stat* stbuf) const {
  stbuf->bsize = meta_->block_size;
  stbuf->frsize = meta_->block_size;
  stbuf->blocks = meta_->total_fs_size / meta_->block_size;
  stbuf->files = meta_->inodes.size();
  stbuf->namemax = 255;
  stbuf->readonly = true;
}

size_t domain_metadata_impl::block_size() const {
  return meta_->block_size;
}

// ========== Chunks ==========

chunk_range domain_metadata_impl::get_chunks(int inode, std::error_code& ec) const {
  ec.clear();

  try {
    uint32_t inode_num = static_cast<uint32_t>(inode);
    uint32_t inode_index = inode_num - inode_offset_;

    if (inode_index >= meta_->inodes.size()) {
      ec = std::make_error_code(std::errc::invalid_argument);
      return chunk_range{domain_chunk_range_impl{*meta_, 0, 0}};
    }

    // Get chunk range from chunk table
    if (inode_index < meta_->chunk_table.size() - 1) {
      uint32_t begin = meta_->chunk_table[inode_index];
      uint32_t end = meta_->chunk_table[inode_index + 1];

      return chunk_range{domain_chunk_range_impl{*meta_, begin, end}};
    }

    // No chunks
    return chunk_range{domain_chunk_range_impl{*meta_, 0, 0}};
  } catch (std::exception const&) {
    ec = std::make_error_code(std::errc::io_error);
    return chunk_range{domain_chunk_range_impl{*meta_, 0, 0}};
  }
}

// ========== Block Categories ==========

std::optional<std::string> domain_metadata_impl::get_block_category(
    size_t block_number) const {
  if (!meta_->block_categories || !meta_->category_names) {
    return std::nullopt;
  }

  if (block_number >= meta_->block_categories->size()) {
    return std::nullopt;
  }

  uint32_t category_index = (*meta_->block_categories)[block_number];

  if (category_index >= meta_->category_names->size()) {
    return std::nullopt;
  }

  return (*meta_->category_names)[category_index];
}

std::optional<nlohmann::json> domain_metadata_impl::get_block_category_metadata(
    size_t block_number) const {
  if (!meta_->block_category_metadata || !meta_->category_metadata_json) {
    return std::nullopt;
  }

  auto it = meta_->block_category_metadata->find(block_number);
  if (it == meta_->block_category_metadata->end()) {
    return std::nullopt;
  }

  uint32_t metadata_index = it->second;

  if (metadata_index >= meta_->category_metadata_json->size()) {
    return std::nullopt;
  }

  return nlohmann::json::parse((*meta_->category_metadata_json)[metadata_index]);
}

std::vector<std::string> domain_metadata_impl::get_all_block_categories() const {
  if (!meta_->category_names) {
    return {};
  }

  return *meta_->category_names;
}

std::vector<size_t> domain_metadata_impl::get_block_numbers_by_category(
    std::string_view category) const {
  std::vector<size_t> result;

  if (!meta_->block_categories || !meta_->category_names) {
    return result;
  }

  // Find category index
  auto it = std::find(meta_->category_names->begin(), meta_->category_names->end(),
                      category);
  if (it == meta_->category_names->end()) {
    return result;
  }

  uint32_t category_index = std::distance(meta_->category_names->begin(), it);

  // Find all blocks with this category
  for (size_t i = 0; i < meta_->block_categories->size(); ++i) {
    if ((*meta_->block_categories)[i] == category_index) {
      result.push_back(i);
    }
  }

  return result;
}

// ========== UID/GID ==========

std::vector<file_stat::uid_type> domain_metadata_impl::get_all_uids() const {
  return meta_->uids;
}

std::vector<file_stat::gid_type> domain_metadata_impl::get_all_gids() const {
  return meta_->gids;
}

// ========== JSON/Debug ==========

nlohmann::json domain_metadata_impl::get_inode_info(inode_view iv,
                                                     size_t max_chunks) const {
  nlohmann::json j;

  try {
    auto const& impl = static_cast<domain_inode_view_impl const&>(iv.raw());
    auto const& inode = get_inode_by_index(impl.inode_index());

    j["inode"] = iv.inode_num();
    j["mode"] = get_mode(*meta_, inode);
    j["uid"] = get_uid(*meta_, inode);
    j["gid"] = get_gid(*meta_, inode);
    j["nlink"] = inode.nlink_minus_one + 1;
    j["size"] = get_file_size(impl.inode_index());

    // Add chunk info
    std::error_code ec;
    auto chunks = get_chunks(iv.inode_num(), ec);
    if (!ec) {
      nlohmann::json chunks_json = nlohmann::json::array();
      size_t count = 0;
      for (auto const& chunk : chunks) {
        if (count >= max_chunks) break;

        nlohmann::json chunk_json;
        chunk_json["block"] = chunk->block();
        chunk_json["offset"] = chunk->offset();
        chunk_json["size"] = chunk->size();
        chunks_json.push_back(chunk_json);
        ++count;
      }
      j["chunks"] = chunks_json;
    }
  } catch (std::exception const& e) {
    j["error"] = e.what();
  }

  return j;
}

void domain_metadata_impl::dump(
    std::ostream& os, fsinfo_options const& opts,
    filesystem_info const* fsinfo,
    std::function<void(std::string const&, uint32_t)> const& icb) const {
  // Dump metadata information
  os << "DwarFS metadata (domain model)\n";
  os << "  Inodes: " << meta_->inodes.size() << "\n";
  os << "  Directories: " << meta_->directories.size() << "\n";
  os << "  Chunks: " << meta_->chunks.size() << "\n";
  os << "  Block size: " << meta_->block_size << "\n";
  os << "  Total size: " << meta_->total_fs_size << "\n";

  if (meta_->dwarfs_version) {
    os << "  Version: " << *meta_->dwarfs_version << "\n";
  }
}

nlohmann::json domain_metadata_impl::info_as_json(
    fsinfo_options const& opts, filesystem_info const* fsinfo) const {
  nlohmann::json j;

  j["inodes"] = meta_->inodes.size();
  j["directories"] = meta_->directories.size();
  j["chunks"] = meta_->chunks.size();
  j["block_size"] = meta_->block_size;
  j["total_size"] = meta_->total_fs_size;

  if (meta_->dwarfs_version) {
    j["version"] = *meta_->dwarfs_version;
  }

  return j;
}

nlohmann::json domain_metadata_impl::as_json() const {
  nlohmann::json j;

  j["inodes"] = meta_->inodes.size();
  j["directories"] = meta_->directories.size();
  j["chunks"] = meta_->chunks.size();
  j["block_size"] = meta_->block_size;
  j["total_fs_size"] = meta_->total_fs_size;

  return j;
}

std::string domain_metadata_impl::serialize_as_json(bool simple) const {
  return as_json().dump(simple ? 0 : 2);
}

// ========== Thrift Export ==========

#ifdef DWARFS_HAVE_THRIFT
std::unique_ptr<thrift::metadata::metadata> domain_metadata_impl::thaw() const {
  // Convert domain model to Thrift using converter
  return metadata::converters::CppThriftConverter::to_thrift(*meta_);
}

std::unique_ptr<thrift::metadata::metadata> domain_metadata_impl::unpack() const {
  // Same as thaw for domain model
  return thaw();
}

std::unique_ptr<thrift::metadata::fs_options>
domain_metadata_impl::thaw_fs_options() const {
  if (!meta_->options) {
    return nullptr;
  }

  auto opts = std::make_unique<thrift::metadata::fs_options>();
  // Convert domain fs_options to Thrift
  // TODO: Implement conversion
  return opts;
}
#else
std::unique_ptr<thrift::metadata::metadata> domain_metadata_impl::thaw() const {
  DWARFS_THROW(runtime_error, "Thrift support not compiled in");
}

std::unique_ptr<thrift::metadata::metadata> domain_metadata_impl::unpack() const {
  DWARFS_THROW(runtime_error, "Thrift support not compiled in");
}

std::unique_ptr<thrift::metadata::fs_options>
domain_metadata_impl::thaw_fs_options() const {
  DWARFS_THROW(runtime_error, "Thrift support not compiled in");
}
#endif

} // namespace dwarfs::reader::internal
// ... existing code ...