/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * Domain-based metadata view wrappers - implementation
 */

#include <dwarfs/reader/internal/domain_metadata_views.h>
#include <dwarfs/file_stat.h>

namespace dwarfs::reader::internal {

// ========== domain_global_metadata ==========

#if !defined(DWARFS_HAVE_FLATBUFFERS) || !defined(DWARFS_HAVE_THRIFT)
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
  return meta_.directories[dir_inode].self_entry();
}

#if defined(DWARFS_HAVE_FLATBUFFERS) && defined(DWARFS_HAVE_THRIFT)
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

std::string domain_global_metadata::name_at(uint32_t index) const {
  return index < meta_.names.size() ? meta_.names[index] : "";
}

std::string domain_global_metadata::symlink_at(uint32_t index) const {
  return index < meta_.symlinks.size() ? meta_.symlinks[index] : "";
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
    : meta_{meta}, inode_index_{inode_index}, inode_num_{inode_num} {}

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
    uint32_t parent_index)
    : meta_{meta}, self_index_{self_index}, parent_index_{parent_index} {}

std::string domain_dir_entry_view_impl::name() const {
  if (!meta_.dir_entries || self_index_ >= meta_.dir_entries->size()) {
    return {};
  }
  auto const& entry = (*meta_.dir_entries)[self_index_];
  uint32_t name_idx = entry.name_index();
  if (name_idx >= meta_.names.size()) {
    return {};
  }
  return meta_.names[name_idx];
}

std::shared_ptr<inode_view_interface>
domain_dir_entry_view_impl::inode() const {
  if (!meta_.dir_entries || self_index_ >= meta_.dir_entries->size()) {
    return nullptr;
  }
  auto const& entry = (*meta_.dir_entries)[self_index_];
  uint32_t inode_num = entry.inode_num();

  // Handle v2.2 vs v2.3 format differences
  uint32_t inode_index;
  if (meta_.entry_table_v2_2.empty()) {
    // v2.3+ format: inode_num IS the inode index (direct mapping)
    inode_index = inode_num;
  } else {
    // v2.2 format: use entry_table_v2_2 to map inode_num to inode_index
    if (inode_num >= meta_.entry_table_v2_2.size()) {
      return nullptr;  // Invalid inode_num
    }
    inode_index = meta_.entry_table_v2_2[inode_num];
  }

  return std::make_shared<domain_inode_view_impl>(meta_, inode_index,
                                                   inode_num);
}

std::shared_ptr<inode_view_interface const>
domain_dir_entry_view_impl::inode_shared() const {
  return inode();
}

uint32_t domain_dir_entry_view_impl::self_index() const { return self_index_; }

uint32_t domain_dir_entry_view_impl::parent_index() const {
  return parent_index_;
}

bool domain_dir_entry_view_impl::is_root() const {
  return self_index_ == 0 && parent_index_ == 0;
}

std::string domain_dir_entry_view_impl::path() const {
  // Build full path by traversing up the directory tree
  std::vector<std::string> components;
  uint32_t current = self_index_;
  uint32_t parent = parent_index_;

  // Traverse up to root, collecting names
  while (current != 0 || parent != 0) {
    // Get current entry's name
    if (!meta_.dir_entries || current >= meta_.dir_entries->size()) {
      break;
    }
    auto const& entry = (*meta_.dir_entries)[current];
    uint32_t name_idx = entry.name_index();
    if (name_idx >= meta_.names.size()) {
      break;
    }
    components.push_back(meta_.names[name_idx]);

    // Stop if we reached root (parent == current)
    if (parent == current) {
      break;
    }

    // Move up to parent
    current = parent;

    // Find parent's parent by looking up parent's inode in directories
    uint32_t parent_inode_num = (*meta_.dir_entries)[current].inode_num();
    uint32_t parent_inode_index;
    if (meta_.entry_table_v2_2.empty()) {
      parent_inode_index = parent_inode_num;
    } else {
      if (parent_inode_num >= meta_.entry_table_v2_2.size()) {
        break;
      }
      parent_inode_index = meta_.entry_table_v2_2[parent_inode_num];
    }

    // Get parent directory's parent_entry
    if (parent_inode_index >= meta_.directories.size()) {
      break;
    }
    parent = meta_.directories[parent_inode_index].parent_entry();
  }

  // Build path from components (reverse order)
  if (components.empty()) {
    return "/";
  }

  std::string result;
  for (auto it = components.rbegin(); it != components.rend(); ++it) {
    result += "/";
    result += *it;
  }

  return result.empty() ? "/" : result;
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

} // namespace dwarfs::reader::internal