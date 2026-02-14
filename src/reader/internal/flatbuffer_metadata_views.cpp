/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \author     Ribose Inc.
 * \copyright  Copyright (c) Ribose Inc.
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

// FlatBuffers wrapper implementations providing Thrift-compatible API

#ifdef DWARFS_HAVE_FLATBUFFERS

#include <dwarfs/reader/internal/flatbuffer_metadata_views.h>
#include <dwarfs/file_stat.h>
#include <dwarfs/fstypes.h>

namespace dwarfs::reader::internal {

//==============================================================================
// flatbuffer_fs_options_view implementation
//==============================================================================

bool flatbuffer_fs_options_view::mtime_only() const {
  return opts_ ? opts_->mtime_only() : false;
}

bool flatbuffer_fs_options_view::packed_chunk_table() const {
  return opts_ ? opts_->packed_chunk_table() : false;
}

bool flatbuffer_fs_options_view::packed_directories() const {
  return opts_ ? opts_->packed_directories() : false;
}

bool flatbuffer_fs_options_view::packed_shared_files_table() const {
  return opts_ ? opts_->packed_shared_files_table() : false;
}

bool flatbuffer_fs_options_view::has_btime() const {
  return opts_ ? opts_->has_btime() : false;
}

bool flatbuffer_fs_options_view::inodes_have_nlink() const {
  return opts_ ? opts_->inodes_have_nlink() : false;
}

uint32_t flatbuffer_fs_options_view::time_resolution_sec() const {
  return opts_ ? opts_->time_resolution_sec() : 1;
}

uint32_t flatbuffer_fs_options_view::subsecond_resolution_nsec_multiplier() const {
  return opts_ ? opts_->subsecond_resolution_nsec_multiplier() : 0;
}

//==============================================================================
// flatbuffer_metadata_view implementation
//==============================================================================

auto flatbuffer_metadata_view::chunks() const
    -> ::flatbuffers::Vector<::flatbuffers::Offset<::dwarfs::flatbuffers::Chunk>> const* {
  return meta_ ? meta_->chunks() : nullptr;
}

auto flatbuffer_metadata_view::directories() const
    -> ::flatbuffers::Vector<::flatbuffers::Offset<::dwarfs::flatbuffers::Directory>> const* {
  return meta_ ? meta_->directories() : nullptr;
}

auto flatbuffer_metadata_view::inodes() const
    -> ::flatbuffers::Vector<::flatbuffers::Offset<::dwarfs::flatbuffers::InodeData>> const* {
  return meta_ ? meta_->inodes() : nullptr;
}

auto flatbuffer_metadata_view::chunk_table() const
    -> ::flatbuffers::Vector<uint32_t> const* {
  return meta_ ? meta_->chunk_table() : nullptr;
}

auto flatbuffer_metadata_view::entry_table_v2_2() const
    -> ::flatbuffers::Vector<uint32_t> const* {
  return meta_ ? meta_->entry_table_v2_2() : nullptr;
}

auto flatbuffer_metadata_view::symlink_table() const
    -> ::flatbuffers::Vector<uint32_t> const* {
  return meta_ ? meta_->symlink_table() : nullptr;
}

auto flatbuffer_metadata_view::dir_entries() const
    -> std::optional<::flatbuffers::Vector<::flatbuffers::Offset<::dwarfs::flatbuffers::DirEntry>> const*> {
  return meta_ && meta_->dir_entries() ? std::optional(meta_->dir_entries()) : std::nullopt;
}

auto flatbuffer_metadata_view::shared_files_table() const
    -> std::optional<::flatbuffers::Vector<uint32_t> const*> {
  return meta_ && meta_->shared_files_table() ? std::optional(meta_->shared_files_table()) : std::nullopt;
}

auto flatbuffer_metadata_view::uids() const
    -> ::flatbuffers::Vector<uint32_t> const* {
  return meta_ ? meta_->uids() : nullptr;
}

auto flatbuffer_metadata_view::gids() const
    -> ::flatbuffers::Vector<uint32_t> const* {
  return meta_ ? meta_->gids() : nullptr;
}

auto flatbuffer_metadata_view::modes() const
    -> ::flatbuffers::Vector<uint32_t> const* {
  return meta_ ? meta_->modes() : nullptr;
}

auto flatbuffer_metadata_view::names() const
    -> ::flatbuffers::Vector<::flatbuffers::Offset<::flatbuffers::String>> const* {
  return meta_ ? meta_->names() : nullptr;
}

auto flatbuffer_metadata_view::symlinks() const
    -> ::flatbuffers::Vector<::flatbuffers::Offset<::flatbuffers::String>> const* {
  return meta_ ? meta_->symlinks() : nullptr;
}

auto flatbuffer_metadata_view::devices() const
    -> std::optional<::flatbuffers::Vector<uint64_t> const*> {
  return meta_ && meta_->devices() ? std::optional(meta_->devices()) : std::nullopt;
}

uint64_t flatbuffer_metadata_view::timestamp_base() const {
  return meta_ ? meta_->timestamp_base() : 0;
}

uint32_t flatbuffer_metadata_view::block_size() const {
  return meta_ ? meta_->block_size() : 0;
}

uint64_t flatbuffer_metadata_view::total_fs_size() const {
  return meta_ ? meta_->total_fs_size() : 0;
}

std::optional<uint64_t> flatbuffer_metadata_view::total_allocated_fs_size() const {
  return meta_ && meta_->total_allocated_fs_size() != 0
      ? std::optional(meta_->total_allocated_fs_size())
      : std::nullopt;
}

std::optional<flatbuffer_fs_options_view> flatbuffer_metadata_view::options() const {
  return meta_ && meta_->options()
      ? std::optional(flatbuffer_fs_options_view(meta_->options()))
      : std::nullopt;
}

std::optional<uint32_t> flatbuffer_metadata_view::hole_block_index() const {
  return meta_ && meta_->hole_block_index() != 0
      ? std::optional(meta_->hole_block_index())
      : std::nullopt;
}

auto flatbuffer_metadata_view::large_hole_size() const
    -> std::optional<::flatbuffers::Vector<uint64_t> const*> {
  return meta_ && meta_->large_hole_size()
      ? std::optional(meta_->large_hole_size())
      : std::nullopt;
}

auto flatbuffer_metadata_view::features() const
    -> std::optional<::flatbuffers::Vector<::flatbuffers::Offset<::flatbuffers::String>> const*> {
  return meta_ && meta_->features()
      ? std::optional(meta_->features())
      : std::nullopt;
}

std::optional<::dwarfs::flatbuffers::StringTable const*> flatbuffer_metadata_view::compact_names() const {
  return meta_ && meta_->compact_names()
      ? std::optional(meta_->compact_names())
      : std::nullopt;
}

std::optional<::dwarfs::flatbuffers::StringTable const*> flatbuffer_metadata_view::compact_symlinks() const {
  return meta_ && meta_->compact_symlinks()
      ? std::optional(meta_->compact_symlinks())
      : std::nullopt;
}

std::optional<std::string_view> flatbuffer_metadata_view::dwarfs_version() const {
  return meta_ && meta_->dwarfs_version()
      ? std::optional(std::string_view(meta_->dwarfs_version()->str()))
      : std::nullopt;
}

std::optional<uint64_t> flatbuffer_metadata_view::create_timestamp() const {
  return meta_ && meta_->create_timestamp() != 0
      ? std::optional(meta_->create_timestamp())
      : std::nullopt;
}

std::optional<uint32_t> flatbuffer_metadata_view::preferred_path_separator() const {
  return meta_ && meta_->preferred_path_separator() != 0
      ? std::optional(static_cast<uint32_t>(meta_->preferred_path_separator()))
      : std::nullopt;
}

auto flatbuffer_metadata_view::category_names() const
    -> std::optional<::flatbuffers::Vector<::flatbuffers::Offset<::flatbuffers::String>> const*> {
  return meta_ && meta_->category_names()
      ? std::optional(meta_->category_names())
      : std::nullopt;
}

auto flatbuffer_metadata_view::block_categories() const
    -> std::optional<::flatbuffers::Vector<uint32_t> const*> {
  return meta_ && meta_->block_categories()
      ? std::optional(meta_->block_categories())
      : std::nullopt;
}

auto flatbuffer_metadata_view::category_metadata_json() const
    -> std::optional<::flatbuffers::Vector<::flatbuffers::Offset<::flatbuffers::String>> const*> {
  return meta_ && meta_->category_metadata_json()
      ? std::optional(meta_->category_metadata_json())
      : std::nullopt;
}

std::optional<::dwarfs::flatbuffers::InodeSizeCache const*> flatbuffer_metadata_view::reg_file_size_cache() const {
  return meta_ && meta_->reg_file_size_cache()
      ? std::optional(meta_->reg_file_size_cache())
      : std::nullopt;
}

auto flatbuffer_metadata_view::metadata_version_history() const
    -> std::optional<::flatbuffers::Vector<::flatbuffers::Offset<::dwarfs::flatbuffers::HistoryEntry>> const*> {
  return meta_ && meta_->metadata_version_history()
      ? std::optional(meta_->metadata_version_history())
      : std::nullopt;
}

std::optional<phmap::flat_hash_map<uint32_t, uint32_t>> flatbuffer_metadata_view::block_category_metadata() const {
  if (!meta_ || !block_cat_meta_cache_.has_value()) {
    // Build cache from parallel arrays on first access
    if (meta_ && meta_->block_category_metadata_keys() && meta_->block_category_metadata_values()) {
      phmap::flat_hash_map<uint32_t, uint32_t> cache;
      auto keys = meta_->block_category_metadata_keys();
      auto vals = meta_->block_category_metadata_values();
      for (size_t i = 0; i < keys->size() && i < vals->size(); ++i) {
        cache[keys->Get(i)] = vals->Get(i);
      }
      block_cat_meta_cache_ = std::move(cache);
    } else {
      return std::nullopt;
    }
  }
  return block_cat_meta_cache_;
}

//==============================================================================
// flatbuffer_chunk_view implementation
//==============================================================================

flatbuffer_chunk_view::flatbuffer_chunk_view(
    ::dwarfs::flatbuffers::Chunk const* chunk,
    std::optional<uint32_t> hole_block_index,
    uint32_t block_size,
    void const* large_hole_sizes)
    : chunk_{chunk}
    , hole_block_index_{hole_block_index}
    , block_size_{block_size}
    , large_hole_sizes_{large_hole_sizes} {
}

uint32_t flatbuffer_chunk_view::block() const {
  return chunk_ ? chunk_->block() : 0;
}

uint32_t flatbuffer_chunk_view::offset() const {
  return chunk_ ? chunk_->offset() : 0;
}

file_off_t flatbuffer_chunk_view::size() const {
  if (!chunk_) return 0;

  // Check if this is a hole chunk
  if (hole_block_index_ && chunk_->block() == *hole_block_index_) {
    // Hole size is stored in offset field for small holes
    uint32_t size_val = chunk_->offset();
    if (size_val == 0 && large_hole_sizes_) {
      // Large hole - lookup in large_hole_size vector
      auto large_holes = static_cast<::flatbuffers::Vector<uint64_t> const*>(large_hole_sizes_);
      if (large_holes && chunk_->size() < large_holes->size()) {
        return static_cast<file_off_t>(large_holes->Get(chunk_->size()));
      }
    }
    return static_cast<file_off_t>(size_val);
  }

  // Data chunk - size is stored in size field
  return static_cast<file_off_t>(chunk_->size());
}

bool flatbuffer_chunk_view::is_data() const {
  return chunk_ && (!hole_block_index_ || chunk_->block() != *hole_block_index_);
}

bool flatbuffer_chunk_view::is_hole() const {
  return !is_data();
}

//==============================================================================
// flatbuffer_chunk_range implementation
//==============================================================================

flatbuffer_chunk_range::flatbuffer_chunk_range(
    ::dwarfs::flatbuffers::Metadata const* metadata,
    uint32_t begin_index, uint32_t end_index)
    : metadata_{metadata}
    , begin_index_{begin_index}
    , end_index_{end_index} {
}

size_t flatbuffer_chunk_range::size() const {
  return end_index_ > begin_index_ ? end_index_ - begin_index_ : 0;
}

bool flatbuffer_chunk_range::empty() const {
  return begin_index_ >= end_index_;
}

std::shared_ptr<chunk_view_interface const> flatbuffer_chunk_range::at(size_t index) const {
  if (!metadata_ || begin_index_ + index >= end_index_) {
    return nullptr;
  }

  auto chunks = metadata_->chunks();
  if (!chunks || begin_index_ + index >= chunks->size()) {
    return nullptr;
  }

  auto chunk = chunks->Get(begin_index_ + index);
  std::optional<uint32_t> hole_idx = metadata_->hole_block_index() != 0
      ? std::optional(metadata_->hole_block_index())
      : std::nullopt;

  return std::make_shared<flatbuffer_chunk_view>(
      chunk, hole_idx, metadata_->block_size(),
      metadata_->large_hole_size());
}

#if defined(DWARFS_HAVE_FLATBUFFERS) && defined(DWARFS_HAVE_EXPERIMENTAL_THRIFT)
flatbuffer_chunk_range::iterator flatbuffer_chunk_range::native_begin() const {
  return iterator(this, 0);
}

flatbuffer_chunk_range::iterator flatbuffer_chunk_range::native_end() const {
  return iterator(this, size());
}

chunk_range_interface::iterator flatbuffer_chunk_range::begin() const {
  return chunk_range_interface::iterator{
      std::make_unique<iterator_impl>(this, 0)};
}

chunk_range_interface::iterator flatbuffer_chunk_range::end() const {
  return chunk_range_interface::iterator{
      std::make_unique<iterator_impl>(this, size())};
}
#else
flatbuffer_chunk_range::iterator flatbuffer_chunk_range::begin() const {
  return iterator(this, 0);
}

flatbuffer_chunk_range::iterator flatbuffer_chunk_range::end() const {
  return iterator(this, size());
}
#endif

//==============================================================================
// flatbuffer_chunk_range::iterator implementation
//==============================================================================

flatbuffer_chunk_range::iterator::iterator(flatbuffer_chunk_range const* range, size_t index)
    : range_{range}
    , index_{index} {
}

flatbuffer_chunk_range::iterator::reference flatbuffer_chunk_range::iterator::operator*() const {
  if (!cached_view_ || !range_) {
    auto view_ptr = range_ ? range_->at(index_) : nullptr;
    if (view_ptr) {
      cached_view_ = std::dynamic_pointer_cast<flatbuffer_chunk_view const>(view_ptr);
    }
  }
  return *cached_view_;
}

flatbuffer_chunk_range::iterator::pointer flatbuffer_chunk_range::iterator::operator->() const {
  return &(**this);
}

flatbuffer_chunk_range::iterator& flatbuffer_chunk_range::iterator::operator++() {
  ++index_;
  cached_view_.reset();
  return *this;
}

flatbuffer_chunk_range::iterator flatbuffer_chunk_range::iterator::operator++(int) {
  iterator tmp = *this;
  ++(*this);
  return tmp;
}

flatbuffer_chunk_range::iterator& flatbuffer_chunk_range::iterator::operator--() {
  --index_;
  cached_view_.reset();
  return *this;
}

flatbuffer_chunk_range::iterator flatbuffer_chunk_range::iterator::operator--(int) {
  iterator tmp = *this;
  --(*this);
  return tmp;
}

flatbuffer_chunk_range::iterator& flatbuffer_chunk_range::iterator::operator+=(difference_type n) {
  index_ += n;
  cached_view_.reset();
  return *this;
}

flatbuffer_chunk_range::iterator& flatbuffer_chunk_range::iterator::operator-=(difference_type n) {
  index_ -= n;
  cached_view_.reset();
  return *this;
}

flatbuffer_chunk_range::iterator flatbuffer_chunk_range::iterator::operator+(difference_type n) const {
  return iterator(range_, index_ + n);
}

flatbuffer_chunk_range::iterator flatbuffer_chunk_range::iterator::operator-(difference_type n) const {
  return iterator(range_, index_ - n);
}

flatbuffer_chunk_range::iterator::difference_type
flatbuffer_chunk_range::iterator::operator-(iterator const& other) const {
  return static_cast<difference_type>(index_) - static_cast<difference_type>(other.index_);
}

bool flatbuffer_chunk_range::iterator::operator==(iterator const& other) const {
  return range_ == other.range_ && index_ == other.index_;
}

bool flatbuffer_chunk_range::iterator::operator!=(iterator const& other) const {
  return !(*this == other);
}

bool flatbuffer_chunk_range::iterator::operator<(iterator const& other) const {
  return index_ < other.index_;
}

bool flatbuffer_chunk_range::iterator::operator<=(iterator const& other) const {
  return index_ <= other.index_;
}

bool flatbuffer_chunk_range::iterator::operator>(iterator const& other) const {
  return index_ > other.index_;
}

bool flatbuffer_chunk_range::iterator::operator>=(iterator const& other) const {
  return index_ >= other.index_;
}

//==============================================================================
// flatbuffer_inode_view implementation
//==============================================================================

flatbuffer_inode_view::flatbuffer_inode_view(
    ::dwarfs::flatbuffers::InodeData const* inode_data,
    uint32_t inode_num,
    ::dwarfs::flatbuffers::Metadata const* metadata)
    : inode_data_{inode_data}
    , inode_num_{inode_num}
    , metadata_{metadata} {
}

flatbuffer_inode_view::mode_type flatbuffer_inode_view::mode() const {
  if (!inode_data_ || !metadata_) return 0;
  auto modes = metadata_->modes();
  return modes ? modes->Get(inode_data_->mode_index()) : 0;
}

std::string flatbuffer_inode_view::mode_string() const {
  return file_stat::mode_string(mode());
}

std::string flatbuffer_inode_view::perm_string() const {
  return file_stat::perm_string(mode());
}

posix_file_type::value flatbuffer_inode_view::type() const {
  return posix_file_type::from_mode(mode());
}

flatbuffer_inode_view::uid_type flatbuffer_inode_view::getuid() const {
  if (!inode_data_ || !metadata_) return 0;
  auto uids = metadata_->uids();
  return uids ? uids->Get(inode_data_->owner_index()) : 0;
}

flatbuffer_inode_view::gid_type flatbuffer_inode_view::getgid() const {
  if (!inode_data_ || !metadata_) return 0;
  auto gids = metadata_->gids();
  return gids ? gids->Get(inode_data_->group_index()) : 0;
}

uint32_t flatbuffer_inode_view::inode_num() const {
  return inode_num_;
}

bool flatbuffer_inode_view::is_directory() const {
  auto m = mode();
  return posix_file_type::from_mode(m) == posix_file_type::directory;
}

//==============================================================================
// flatbuffer_dir_entry_view implementation
//==============================================================================

flatbuffer_dir_entry_view::flatbuffer_dir_entry_view(
    ::dwarfs::flatbuffers::DirEntry const* dir_entry,
    uint32_t self_index, uint32_t parent_index,
    ::dwarfs::flatbuffers::Metadata const* metadata)
    : dir_entry_{dir_entry}
    , self_index_{self_index}
    , parent_index_{parent_index}
    , metadata_{metadata} {
}

std::string flatbuffer_dir_entry_view::name() const {
  if (!dir_entry_ || !metadata_) return "";
  auto names = metadata_->names();
  return names && dir_entry_->name_index() < names->size()
      ? names->Get(dir_entry_->name_index())->str()
      : "";
}

std::shared_ptr<inode_view_interface> flatbuffer_dir_entry_view::inode() const {
  if (!dir_entry_ || !metadata_) {
    return nullptr;
  }

  uint32_t inode_num = dir_entry_->inode_num();
  auto entry_table = metadata_->entry_table_v2_2();
  auto inodes = metadata_->inodes();

  uint32_t index = metadata_->dir_entries() ? inode_num :
                   (entry_table ? entry_table->Get(inode_num) : inode_num);

  auto inode_data = inodes ? inodes->Get(index) : nullptr;
  return std::make_shared<flatbuffer_inode_view>(inode_data, inode_num, metadata_);
}

uint32_t flatbuffer_dir_entry_view::self_index() const {
  return self_index_;
}

uint32_t flatbuffer_dir_entry_view::parent_index() const {
  return parent_index_;
}

bool flatbuffer_dir_entry_view::is_root() const {
  return self_index_ == 0;
}

uint32_t flatbuffer_dir_entry_view::entry_to_dir_idx(uint32_t entry_idx) const {
  // Find the directory that this entry belongs to
  if (!metadata_ || !metadata_->dir_entries() || !metadata_->directories()) {
    return 0;  // Legacy format or no directories, return root
  }

  auto directories = metadata_->directories();

  // First check if this entry IS a directory's self_entry
  // If so, return that directory's index
  for (size_t dir_idx = 0; dir_idx < directories->size(); ++dir_idx) {
    auto const* directory = directories->Get(dir_idx);
    if (directory && directory->self_entry() == entry_idx) {
      return static_cast<uint32_t>(dir_idx);
    }
  }

  // Otherwise find the directory whose range contains this entry
  // This is the same logic used in walk() to determine parent_dir_idx
  uint32_t result = 0;
  for (size_t dir_idx = 0; dir_idx < directories->size(); ++dir_idx) {
    auto const* directory = directories->Get(dir_idx);
    if (directory && directory->first_entry() <= entry_idx) {
      result = dir_idx;
    } else {
      break;
    }
  }
  return result;
}

std::string flatbuffer_dir_entry_view::path() const {
  // For legacy format (no dir_entries), the filesystem is flat (all files in root)
  if (!metadata_ || !metadata_->dir_entries()) {
    // Return just the name for flat filesystem (no leading /)
    return name();
  }

  // Build full path by traversing up the directory tree
  std::vector<std::string> components;
  uint32_t current = self_index_;
  uint32_t parent = parent_index_;

  // Traverse up to root, collecting names
  while (current != 0 || parent != 0) {
    // Get current entry's name
    auto dir_entries = metadata_->dir_entries();
    if (!dir_entries || current >= dir_entries->size()) {
      break;
    }

    auto const* entry = dir_entries->Get(current);
    if (!entry) {
      break;
    }

    uint32_t name_idx = entry->name_index();
    std::string name_from_idx = [this, name_idx]() -> std::string {
      auto names = metadata_->names();
      return names && name_idx < names->size() ? names->Get(name_idx)->str() : "";
    }();

    if (name_from_idx.empty()) {
      break;
    }
    components.push_back(name_from_idx);

    // Stop if we reached root
    if (parent == current || parent == 0) {
      break;
    }

    // Move up to parent
    current = parent;

    // CRITICAL: Convert entry index to directory index before looking up parent_entry
    uint32_t parent_dir_idx = entry_to_dir_idx(current);

    auto directories = metadata_->directories();
    if (!directories || parent_dir_idx >= directories->size()) {
      break;
    }

    auto const* directory = directories->Get(parent_dir_idx);
    if (!directory) {
      break;
    }

    parent = directory->parent_entry();
  }

  // Build path from components (reverse order)
  if (components.empty()) {
    return "";  // Root
  }

  // Join components with /
  std::string result;
  for (auto it = components.rbegin(); it != components.rend(); ++it) {
    if (!result.empty()) {
      result += "/";
    }
    result += *it;
  }
  return result;
}

//==============================================================================
// flatbuffer_directory_view implementation
//==============================================================================

flatbuffer_directory_view::flatbuffer_directory_view(
    ::dwarfs::flatbuffers::Directory const* directory,
    uint32_t inode_num,
    ::dwarfs::flatbuffers::Metadata const* metadata)
    : directory_{directory}
    , inode_num_{inode_num}
    , metadata_{metadata} {
}

uint32_t flatbuffer_directory_view::inode() const {
  return inode_num_;
}

size_t flatbuffer_directory_view::entry_count() const {
  if (!directory_ || !metadata_) return 0;

  // Calculate entry_count from the difference between successive directories' first_entry values
  // This follows the same pattern as Thrift's packed directories
  auto directories = metadata_->directories();
  if (!directories || inode_num_ >= directories->size() - 1) return 0;

  // The sentinel directory at the end has first_entry pointing to end of dir_entries
  uint32_t next_first = directories->Get(inode_num_ + 1)->first_entry();
  uint32_t curr_first = directory_->first_entry();

  return next_first > curr_first ? next_first - curr_first : 0;
}

uint32_t flatbuffer_directory_view::first_entry() const {
  return directory_ ? directory_->first_entry() : 0;
}

uint32_t flatbuffer_directory_view::parent_entry() const {
  return directory_ ? directory_->parent_entry() : 0;
}

uint32_t flatbuffer_directory_view::self_entry() const {
  // In FlatBuffers, self_entry is typically the inode number itself
  return inode_num_;
}

//==============================================================================
// flatbuffer_global_metadata implementation
//==============================================================================

flatbuffer_global_metadata::flatbuffer_global_metadata(
    ::dwarfs::flatbuffers::Metadata const* metadata)
    : metadata_{metadata} {
}

std::span<uint8_t const> flatbuffer_global_metadata::uids() const {
  if (!metadata_) return {};
  auto uids = metadata_->uids();
  return uids ? std::span<uint8_t const>(
      reinterpret_cast<uint8_t const*>(uids->data()),
      uids->size() * sizeof(uint32_t)) : std::span<uint8_t const>{};
}

std::span<uint8_t const> flatbuffer_global_metadata::gids() const {
  if (!metadata_) return {};
  auto gids = metadata_->gids();
  return gids ? std::span<uint8_t const>(
      reinterpret_cast<uint8_t const*>(gids->data()),
      gids->size() * sizeof(uint32_t)) : std::span<uint8_t const>{};
}

std::span<uint8_t const> flatbuffer_global_metadata::modes() const {
  if (!metadata_) return {};
  auto modes = metadata_->modes();
  return modes ? std::span<uint8_t const>(
      reinterpret_cast<uint8_t const*>(modes->data()),
      modes->size() * sizeof(uint32_t)) : std::span<uint8_t const>{};
}

std::string flatbuffer_global_metadata::name_at(uint32_t index) const {
  if (!metadata_) return "";
  auto names = metadata_->names();
  return names && index < names->size() ? names->Get(index)->str() : "";
}

std::string flatbuffer_global_metadata::symlink_at(uint32_t index) const {
  if (!metadata_) return "";
  auto symlinks = metadata_->symlinks();
  return symlinks && index < symlinks->size() ? symlinks->Get(index)->str() : "";
}

uint32_t flatbuffer_global_metadata::block_size() const {
  return metadata_ ? metadata_->block_size() : 0;
}

uint64_t flatbuffer_global_metadata::total_fs_size() const {
  return metadata_ ? metadata_->total_fs_size() : 0;
}

std::optional<uint32_t> flatbuffer_global_metadata::hole_block_index() const {
  return metadata_ && metadata_->hole_block_index() != 0
      ? std::optional(metadata_->hole_block_index())
      : std::nullopt;
}

} // namespace dwarfs::reader::internal

#endif // DWARFS_HAVE_FLATBUFFERS