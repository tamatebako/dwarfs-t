/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * Domain-based metadata view wrappers - implementation
 */

#include <dwarfs/reader/internal/domain_metadata_views.h>
#include <dwarfs/file_stat.h>

namespace dwarfs::reader::internal {

// ========== domain_global_metadata ==========

#if !defined(DWARFS_HAVE_FLATBUFFERS) || !defined(DWARFS_HAVE_EXPERIMENTAL_THRIFT)
// Non-interface implementations for single-format builds
std::shared_ptr<domain_dir_entry_view_impl const>
domain_global_metadata::make_dir_entry_view(uint32_t self_index,
                                             uint32_t parent_index) const {
  return std::make_shared<domain_dir_entry_view_impl>(
      meta_, self_index, parent_index);
}

uint32_t domain_global_metadata::first_dir_entry(uint32_t dir_inode) const {
  if (dir_inode >= meta_.directories.size()) {
    return 0;
  }
  return meta_.directories[dir_inode].first_entry();
}

uint32_t domain_global_metadata::parent_dir_entry(uint32_t dir_inode) const {
  if (dir_inode >= meta_.directories.size()) {
    return 0;
  }

  // For legacy format (no dir_entries), return the parent_entry field directly
  // if it's within bounds, otherwise return the directory index itself as fallback
  if (!meta_.dir_entries) {
    uint32_t parent_entry = meta_.directories[dir_inode].parent_entry();
    // If parent_entry is 0 or equals dir_inode, it's the root or self-referential
    if (parent_entry == 0 || parent_entry >= meta_.directories.size()) {
      return (dir_inode == 0) ? 0 : dir_inode - 1;  // Return previous directory or root
    }
    return parent_entry;
  }

  return meta_.directories[dir_inode].parent_entry();
}
#endif

// self_dir_entry is always needed (not in interface)
uint32_t domain_global_metadata::self_dir_entry(uint32_t dir_inode) const {
  // Map directory inode number to its self_entry index
  // The directory at index dir_inode has a self_entry field that points
  // to the entry index in dir_entries
  if (dir_inode >= meta_.directories.size()) {
    return 0;  // Invalid inode, return root
  }

  // For legacy format (no dir_entries), return the directory index itself
  // instead of the self_entry field (which is meaningless without dir_entries)
  if (!meta_.dir_entries) {
    return dir_inode;  // Use directory index as the entry index
  }

  return meta_.directories[dir_inode].self_entry();
}

// name_at and symlink_at are always needed (for both single and dual-format builds)
std::string domain_global_metadata::name_at(uint32_t index) const {
  // First try legacy names array (for v0.2.3 format)
  if (index < meta_.names.size()) {
    return meta_.names[index];
  }

  // Fall back to compact_names (v2.3+ format)
  if (meta_.compact_names) {
    return read_from_compact_names(*meta_.compact_names, index);
  }

  return "";
}

std::string domain_global_metadata::symlink_at(uint32_t index) const {
  // First try legacy symlinks array (for v0.2.3 format)
  if (index < meta_.symlinks.size()) {
    return meta_.symlinks[index];
  }

  // Fall back to compact_symlinks (v2.3+ format)
  if (meta_.compact_symlinks) {
    return read_from_compact_symlinks(*meta_.compact_symlinks, index);
  }

  return "";
}

#if defined(DWARFS_HAVE_FLATBUFFERS) && defined(DWARFS_HAVE_EXPERIMENTAL_THRIFT)
// Interface implementations for dual-format builds

std::span<uint8_t const> domain_global_metadata::uids() const {
  return std::span<uint8_t const>(
      reinterpret_cast<uint8_t const*>(meta_.uids.data()),
      meta_.uids.size() * sizeof(uint32_t));
}

std::span<uint8_t const> domain_global_metadata::gids() const {
  return std::span<uint8_t const>(
      reinterpret_cast<uint8_t const*>(meta_.gids.data()),
      meta_.gids.size() * sizeof(uint32_t));
}

std::span<uint8_t const> domain_global_metadata::modes() const {
  return std::span<uint8_t const>(
      reinterpret_cast<uint8_t const*>(meta_.modes.data()),
      meta_.modes.size() * sizeof(uint16_t));
}

uint32_t domain_global_metadata::block_size() const {
  return meta_.block_size;
}

uint64_t domain_global_metadata::total_fs_size() const {
  return meta_.total_fs_size;
}

std::optional<uint32_t> domain_global_metadata::hole_block_index() const {
  return meta_.hole_block_index;
}

// Interface override versions
uint32_t domain_global_metadata::first_dir_entry(uint32_t ino) const {
  if (ino >= meta_.directories.size()) {
    return 0;
  }
  return meta_.directories[ino].first_entry();
}

uint32_t domain_global_metadata::parent_dir_entry(uint32_t ino) const {
  if (ino >= meta_.directories.size()) {
    return 0;
  }
  return meta_.directories[ino].parent_entry();
}

std::shared_ptr<dir_entry_view_interface const>
domain_global_metadata::make_dir_entry_view(uint32_t index, uint32_t parent_index) const {
  return std::make_shared<domain_dir_entry_view_impl>(meta_, index, parent_index);
}

std::shared_ptr<dir_entry_view_interface const>
domain_global_metadata::make_dir_entry_view(uint32_t index) const {
  // Use index as both self and parent for single-argument case
  return make_dir_entry_view(index, index);
}

#endif

// ========== domain_inode_view_impl ==========

domain_inode_view_impl::domain_inode_view_impl(
    metadata::domain::metadata const& meta, uint32_t inode_index,
    uint32_t inode_num)
    : meta_{meta}, inode_index_{inode_index}, inode_num_{inode_num} {
}

auto domain_inode_view_impl::mode() const -> mode_type {
  if (inode_index_ >= meta_.inodes.size()) {
    return 0;
  }
  auto const& inode = meta_.inodes[inode_index_];
  if (inode.mode_index >= meta_.modes.size()) {
    return 0;
  }
  return meta_.modes[inode.mode_index];
}

std::string domain_inode_view_impl::mode_string() const {
  return file_stat::mode_string(mode());
}

std::string domain_inode_view_impl::perm_string() const {
  return file_stat::perm_string(mode());
}

posix_file_type::value domain_inode_view_impl::type() const {
  return posix_file_type::from_mode(mode());
}

auto domain_inode_view_impl::getuid() const -> uid_type {
  if (inode_index_ >= meta_.inodes.size()) {
    return 0;
  }
  auto const& inode = meta_.inodes[inode_index_];
  if (inode.owner_index >= meta_.uids.size()) {
    return 0;
  }
  return meta_.uids[inode.owner_index];
}

auto domain_inode_view_impl::getgid() const -> gid_type {
  if (inode_index_ >= meta_.inodes.size()) {
    return 0;
  }
  auto const& inode = meta_.inodes[inode_index_];
  if (inode.group_index >= meta_.gids.size()) {
    return 0;
  }
  return meta_.gids[inode.group_index];
}

uint32_t domain_inode_view_impl::inode_num() const { return inode_num_; }

bool domain_inode_view_impl::is_directory() const {
  return type() == posix_file_type::directory;
}

// ========== domain_dir_entry_view_impl ==========

domain_dir_entry_view_impl::domain_dir_entry_view_impl(
    metadata::domain::metadata const& meta, uint32_t self_index,
    uint32_t parent_index, std::optional<std::string> name_override)
    : meta_{meta},
      self_index_{self_index},
      parent_index_{parent_index},
      name_override_{std::move(name_override)} {
}

// Helper method to read name by index (handles both legacy names and compact_names)
std::string domain_dir_entry_view_impl::name_at(uint32_t index) const {
  // First try legacy names array (for v0.2.3 format)
  if (index < meta_.names.size()) {
    return meta_.names[index];
  }

  // Fall back to compact_names (v2.3+ format)
  if (!meta_.compact_names) {
    return "";
  }
  auto const& table = *meta_.compact_names;
  if (table.index.empty() || index >= table.index.size()) {
    return "";
  }
  uint32_t offset = table.index[index];
  uint32_t end_offset = (index + 1 < table.index.size())
      ? table.index[index + 1]
      : table.buffer.size();
  if (offset > end_offset || end_offset > table.buffer.size()) {
    return "";
  }
  return table.buffer.substr(offset, end_offset - offset);
}

std::string domain_dir_entry_view_impl::name() const {
  // Synthesized entries ("." / "..") carry an explicit name
  if (name_override_) {
    return *name_override_;
  }

  // For legacy format (no dir_entries), names are indexed directly by entry index
  if (!meta_.dir_entries) {
    return name_at(self_index_);
  }

  if (self_index_ >= meta_.dir_entries->size()) {
    return {};
  }
  auto const& entry = (*meta_.dir_entries)[self_index_];
  uint32_t name_idx = entry.name_index();

  // Use name_at() which handles both legacy names and compact_names
  return name_at(name_idx);
}

std::shared_ptr<inode_view_interface>
domain_dir_entry_view_impl::inode() const {
  // Handle legacy mode (no dir_entries)
  if (!meta_.dir_entries.has_value()) {
    uint32_t inode_index;
    uint32_t inode_num;

    // Check if we have entry_table_v2_2 (v0.2.3 format)
    if (!meta_.entry_table_v2_2.empty()) {
      // v0.2.3 format: entry_table_v2_2[entry_idx] = inode_index
      // For v0.2.3, chunk_table is indexed by entry index, not inode_index
      if (self_index_ >= meta_.entry_table_v2_2.size()) {
        return nullptr;
      }
      inode_index = meta_.entry_table_v2_2[self_index_];
      inode_num = self_index_;  // In v0.2.3, inode_num = entry_idx for chunk_table indexing
    } else {
      // Very old format: self_index_ is directly the inode index
      if (self_index_ >= meta_.inodes.size()) {
        return nullptr;
      }
      inode_index = self_index_;
      inode_num = self_index_;
    }
    return std::make_shared<domain_inode_view_impl>(meta_, inode_index, inode_num);
  }

  if (self_index_ >= meta_.dir_entries->size()) {
    return nullptr;
  }
  auto const& entry = (*meta_.dir_entries)[self_index_];
  uint32_t inode_num = entry.inode_num();

  // Handle v2.2 vs v2.3 format differences
  uint32_t inode_index;
  if (meta_.entry_table_v2_2.empty()) {
    // v2.3+ format: inode_num IS the inode index (direct mapping)
    // For modern images created by current writers, dir_entry.inode_num
    // is the direct index into the inodes array.
    inode_index = inode_num;
  } else {
    // v2.2 format: use entry_table_v2_2 to map inode_num to inode_index
    if (inode_num >= meta_.entry_table_v2_2.size()) {
      return nullptr;  // Invalid inode_num
    }
    inode_index = meta_.entry_table_v2_2[inode_num];
  }

  return std::make_shared<domain_inode_view_impl>(meta_, inode_index, inode_num);
}

std::shared_ptr<inode_view_interface const>
domain_dir_entry_view_impl::inode_shared() const {
  return inode();
}

uint32_t domain_dir_entry_view_impl::self_index() const { return self_index_; }

uint32_t domain_dir_entry_view_impl::parent_index() const {
  return parent_index_;
}

uint32_t domain_dir_entry_view_impl::entry_to_dir_idx(uint32_t entry_idx) const {
  // Find the directory that this entry belongs to
  if (!meta_.dir_entries || meta_.directories.empty()) {
    return 0;  // Legacy format or no directories, return root
  }

  // First check if this entry IS a directory's self_entry
  // If so, return that directory's index
  for (size_t dir_idx = 0; dir_idx < meta_.directories.size(); ++dir_idx) {
    if (meta_.directories[dir_idx].self_entry() == entry_idx) {
      return static_cast<uint32_t>(dir_idx);
    }
  }

  // Otherwise find the directory whose range contains this entry
  // This is the same logic used in walk() to determine parent_dir_idx
  uint32_t result = 0;
  for (size_t dir_idx = 0; dir_idx < meta_.directories.size(); ++dir_idx) {
    if (meta_.directories[dir_idx].first_entry() <= entry_idx) {
      result = dir_idx;
    } else {
      break;
    }
  }
  return result;
}

bool domain_dir_entry_view_impl::is_root() const {
  return self_index_ == 0 && parent_index_ == 0;
}

std::string domain_dir_entry_view_impl::path() const {
  // For legacy format (no dir_entries), the filesystem is flat (all files in root)
  // and names are indexed directly by entry index
  if (!meta_.dir_entries) {
    if (self_index_ == 0) {
      return "";  // Return empty string for root so caller can prepend /
    }

    // Return name for flat filesystem (use name_at() to handle compact_names)
    // Note: No leading / so caller can prepend it
    std::string name = name_at(self_index_);
    if (name.empty()) {
      return "";  // Return empty string for root so caller can prepend /
    }
    return name;
  }

  // Build full path by traversing up the directory tree
  std::vector<std::string> components;
  uint32_t current = self_index_;
  uint32_t parent = parent_index_;

  // Traverse up to root, collecting names
  while (current != 0 || parent != 0) {
    // Get current entry's name
    if (current >= meta_.dir_entries->size()) {
      break;
    }
    auto const& entry = (*meta_.dir_entries)[current];
    uint32_t name_idx = entry.name_index();
    std::string name = name_at(name_idx);
    if (name.empty()) {
      break;
    }
    components.push_back(name);

    // Stop if we reached root
    if (parent == current || parent == 0) {
      break;
    }

    // Move up to parent
    current = parent;

    // CRITICAL FIX: Convert entry index to directory index before looking up parent_entry
    uint32_t parent_dir_idx = entry_to_dir_idx(current);

    if (parent_dir_idx >= meta_.directories.size()) {
      break;
    }

    parent = meta_.directories[parent_dir_idx].parent_entry();
  }

  // Build path from components (reverse order)
  if (components.empty()) {
    return "";  // Return empty string for root so caller can prepend /
  }

  // Join components with / (no leading /)
  std::string result;
  for (auto it = components.rbegin(); it != components.rend(); ++it) {
    if (!result.empty()) {
      result += "/";
    }
    result += *it;
  }

  return result;
}

std::string domain_dir_entry_view_impl::unix_path() const {
  // path() already returns Unix-style paths with forward slashes
  return path();
}

std::filesystem::path domain_dir_entry_view_impl::fs_path() const {
  return std::filesystem::path(path());
}

std::wstring domain_dir_entry_view_impl::wpath() const {
  auto p = path();
  return std::wstring(p.begin(), p.end());
}

std::unique_ptr<dir_entry_view_interface>
domain_dir_entry_view_impl::parent() const {
  if (is_root()) {
    return nullptr;  // Root has no parent
  }

  // Create parent entry
  uint32_t parent_inode_num = (*meta_.dir_entries)[parent_index_].inode_num();
  uint32_t parent_inode_index;
  if (meta_.entry_table_v2_2.empty()) {
    parent_inode_index = parent_inode_num;
  } else {
    if (parent_inode_num >= meta_.entry_table_v2_2.size()) {
      return nullptr;
    }
    parent_inode_index = meta_.entry_table_v2_2[parent_inode_num];
  }

  // Get parent's parent
  uint32_t parent_parent_index = 0;
  if (parent_inode_index < meta_.directories.size()) {
    parent_parent_index = meta_.directories[parent_inode_index].parent_entry();
  }

  return std::make_unique<domain_dir_entry_view_impl>(
      meta_, parent_index_, parent_parent_index);
}

// ========== domain_chunk_view ==========

domain_chunk_view::domain_chunk_view(metadata::domain::metadata const& meta,
                                     uint32_t chunk_index)
    : meta_{meta}, chunk_index_{chunk_index} {}

uint32_t domain_chunk_view::block() const {
  if (chunk_index_ >= meta_.chunks.size()) {
    return 0;
  }
  return meta_.chunks[chunk_index_].block();
}

uint32_t domain_chunk_view::offset() const {
  if (chunk_index_ >= meta_.chunks.size()) {
    return 0;
  }
  return meta_.chunks[chunk_index_].offset();
}

file_off_t domain_chunk_view::size() const {
  if (chunk_index_ >= meta_.chunks.size()) {
    return 0;
  }
  return meta_.chunks[chunk_index_].size();
}

bool domain_chunk_view::is_data() const {
  if (!meta_.hole_block_index) {
    return true;  // No holes defined
  }
  return block() != *meta_.hole_block_index;
}

bool domain_chunk_view::is_hole() const { return !is_data(); }

// ========== domain_chunk_range_impl ==========

domain_chunk_range_impl::domain_chunk_range_impl(
    metadata::domain::metadata const& meta, uint32_t begin_index,
    uint32_t end_index)
    : meta_{meta}, begin_index_{begin_index}, end_index_{end_index} {}

size_t domain_chunk_range_impl::size() const {
  return end_index_ - begin_index_;
}

bool domain_chunk_range_impl::empty() const { return size() == 0; }

std::shared_ptr<chunk_view_interface const>
domain_chunk_range_impl::at(size_t index) const {
  uint32_t chunk_index = begin_index_ + index;
  if (chunk_index >= end_index_ || chunk_index >= meta_.chunks.size()) {
    return nullptr;
  }
  return std::make_shared<domain_chunk_view>(meta_, chunk_index);
}

// ========== Helper Functions for Compact String Tables ==========

std::string domain_global_metadata::read_from_compact_names(
    metadata::domain::string_table const& table, uint32_t index) {
  if (table.index.empty()) {
    return "";
  }

  if (index >= table.index.size()) {
    return "";
  }

  // When packed_index=true, the index contains SIZES that need to be converted to offsets
  // When packed_index=false, the index already contains OFFSETS
  uint32_t offset, end_offset;

  if (table.packed_index) {
    // Index contains sizes: [s0, s1, ..., sN-1, buffer_size]
    // Convert to cumulative offset for the requested index
    offset = 0;
    for (size_t i = 0; i < index; ++i) {
      offset += table.index[i];
    }

    // Calculate end offset
    end_offset = offset + table.index[index];

    // Sanity check: end_offset should not exceed buffer size
    if (end_offset > table.buffer.size()) {
      end_offset = table.buffer.size();
    }
  } else {
    // Index contains offsets: [0, o1, o2, ..., buffer_size]
    offset = table.index[index];
    end_offset = (index + 1 < table.index.size())
        ? table.index[index + 1]
        : table.buffer.size();
  }

  // Sanity check
  if (offset > end_offset || end_offset > table.buffer.size()) {
    return "";
  }

  return table.buffer.substr(offset, end_offset - offset);
}

std::string domain_global_metadata::read_from_compact_symlinks(
    metadata::domain::string_table const& table, uint32_t index) {
  if (table.index.empty()) {
    return "";
  }

  if (index >= table.index.size()) {
    return "";
  }

  // When packed_index=true, the index contains SIZES that need to be converted to offsets
  // When packed_index=false, the index already contains OFFSETS
  uint32_t offset, end_offset;

  if (table.packed_index) {
    // Index contains sizes: [s0, s1, ..., sN-1, buffer_size]
    // Convert to cumulative offset for the requested index
    offset = 0;
    for (size_t i = 0; i < index; ++i) {
      offset += table.index[i];
    }

    // Calculate end offset
    end_offset = offset + table.index[index];

    // Sanity check: end_offset should not exceed buffer size
    if (end_offset > table.buffer.size()) {
      end_offset = table.buffer.size();
    }
  } else {
    // Index contains offsets: [0, o1, o2, ..., buffer_size]
    offset = table.index[index];
    end_offset = (index + 1 < table.index.size())
        ? table.index[index + 1]
        : table.buffer.size();
  }

  // Sanity check
  if (offset > end_offset || end_offset > table.buffer.size()) {
    return "";
  }

  return table.buffer.substr(offset, end_offset - offset);
}

} // namespace dwarfs::reader::internal