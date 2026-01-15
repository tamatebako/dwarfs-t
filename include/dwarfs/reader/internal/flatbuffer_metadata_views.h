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

#pragma once

#ifdef DWARFS_HAVE_FLATBUFFERS

// Include FlatBuffers generated header FIRST, before any dwarfs namespaces
// to avoid namespace collision (dwarfs::reader::internal::flatbuffers vs ::flatbuffers)
#include <dwarfs/gen-flatbuffers/metadata.h>

#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include <parallel_hashmap/phmap.h>

#include <dwarfs/file_stat.h>
#include <dwarfs/types.h>

#include <dwarfs/reader/internal/metadata_view_interface.h>

// FlatBuffers types are now in ::dwarfs::flatbuffers namespace (from generated header)

namespace dwarfs::reader::internal {

/**
 * Wrapper for FlatBuffers FsOptions providing Thrift-like API
 */
class flatbuffer_fs_options_view {
 public:
  explicit flatbuffer_fs_options_view(::dwarfs::flatbuffers::FsOptions const* opts)
      : opts_{opts} {}

  bool mtime_only() const;
  bool packed_chunk_table() const;
  bool packed_directories() const;
  bool packed_shared_files_table() const;
  bool has_btime() const;
  bool inodes_have_nlink() const;

  uint32_t time_resolution_sec() const;
  uint32_t subsecond_resolution_nsec_multiplier() const;

 private:
  ::dwarfs::flatbuffers::FsOptions const* opts_;
};

/**
 * Wrapper for FlatBuffers Metadata providing Thrift-like API
 *
 * This class wraps the generated FlatBuffers::Metadata accessor
 * to provide an API compatible with the Thrift frozen view API
 * used throughout metadata_v2_data.
 */
class flatbuffer_metadata_view {
 public:
  explicit flatbuffer_metadata_view(::dwarfs::flatbuffers::Metadata const* meta)
      : meta_{meta} {}

  // Core filesystem structures - use ::flatbuffers for library types, ::dwarfs::flatbuffers for our types
  auto chunks() const -> ::flatbuffers::Vector<::flatbuffers::Offset<::dwarfs::flatbuffers::Chunk>> const*;
  auto directories() const -> ::flatbuffers::Vector<::flatbuffers::Offset<::dwarfs::flatbuffers::Directory>> const*;
  auto inodes() const -> ::flatbuffers::Vector<::flatbuffers::Offset<::dwarfs::flatbuffers::InodeData>> const*;
  auto chunk_table() const -> ::flatbuffers::Vector<uint32_t> const*;
  auto entry_table_v2_2() const -> ::flatbuffers::Vector<uint32_t> const*;
  auto symlink_table() const -> ::flatbuffers::Vector<uint32_t> const*;
  auto dir_entries() const -> std::optional<::flatbuffers::Vector<::flatbuffers::Offset<::dwarfs::flatbuffers::DirEntry>> const*>;
  auto shared_files_table() const -> std::optional<::flatbuffers::Vector<uint32_t> const*>;

  // Lookup tables
  auto uids() const -> ::flatbuffers::Vector<uint32_t> const*;
  auto gids() const -> ::flatbuffers::Vector<uint32_t> const*;
  auto modes() const -> ::flatbuffers::Vector<uint32_t> const*;
  auto names() const -> ::flatbuffers::Vector<::flatbuffers::Offset<::flatbuffers::String>> const*;
  auto symlinks() const -> ::flatbuffers::Vector<::flatbuffers::Offset<::flatbuffers::String>> const*;
  auto devices() const -> std::optional<::flatbuffers::Vector<uint64_t> const*>;

  // Filesystem parameters
  uint64_t timestamp_base() const;
  uint32_t block_size() const;
  uint64_t total_fs_size() const;
  std::optional<uint64_t> total_allocated_fs_size() const;

  // Optional features
  std::optional<flatbuffer_fs_options_view> options() const;
  std::optional<uint32_t> hole_block_index() const;
  std::optional<::flatbuffers::Vector<uint64_t> const*> large_hole_size() const;
  std::optional<::flatbuffers::Vector<::flatbuffers::Offset<::flatbuffers::String>> const*> features() const;

  // Compact string storage
  std::optional<::dwarfs::flatbuffers::StringTable const*> compact_names() const;
  std::optional<::dwarfs::flatbuffers::StringTable const*> compact_symlinks() const;

  // Metadata information
  std::optional<std::string_view> dwarfs_version() const;
  std::optional<uint64_t> create_timestamp() const;
  std::optional<uint32_t> preferred_path_separator() const;

  // Categories
  std::optional<::flatbuffers::Vector<::flatbuffers::Offset<::flatbuffers::String>> const*> category_names() const;
  std::optional<::flatbuffers::Vector<uint32_t> const*> block_categories() const;
  std::optional<::flatbuffers::Vector<::flatbuffers::Offset<::flatbuffers::String>> const*> category_metadata_json() const;

  // Size cache
  std::optional<::dwarfs::flatbuffers::InodeSizeCache const*> reg_file_size_cache() const;

  // History
  std::optional<::flatbuffers::Vector<::flatbuffers::Offset<::dwarfs::flatbuffers::HistoryEntry>> const*> metadata_version_history() const;

  // Block category metadata (stored as two parallel arrays)
  std::optional<phmap::flat_hash_map<uint32_t, uint32_t>> block_category_metadata() const;

  // Raw pointer access
  ::dwarfs::flatbuffers::Metadata const* raw() const { return meta_; }

 private:
  ::dwarfs::flatbuffers::Metadata const* meta_;
  mutable std::optional<phmap::flat_hash_map<uint32_t, uint32_t>> block_cat_meta_cache_;
};

/**
 * FlatBuffer-based chunk view
 */
class flatbuffer_chunk_view : public chunk_view_interface {
 public:
  flatbuffer_chunk_view(::dwarfs::flatbuffers::Chunk const* chunk,
                        std::optional<uint32_t> hole_block_index,
                        uint32_t block_size,
                        void const* large_hole_sizes);

  uint32_t block() const override;
  uint32_t offset() const override;
  file_off_t size() const override;
  bool is_data() const override;
  bool is_hole() const override;

 private:
  ::dwarfs::flatbuffers::Chunk const* chunk_;
  std::optional<uint32_t> hole_block_index_;
  uint32_t block_size_;
  void const* large_hole_sizes_;
};

/**
 * FlatBuffer-based chunk range
 */
class flatbuffer_chunk_range
#if defined(DWARFS_HAVE_FLATBUFFERS) && defined(DWARFS_HAVE_THRIFT)
    : public chunk_range_interface  // Only inherit in dual-format builds
#endif
{
 public:
  class iterator;  // Forward declare

  flatbuffer_chunk_range(::dwarfs::flatbuffers::Metadata const* metadata,
                         uint32_t begin_index, uint32_t end_index);

  size_t size() const
#if defined(DWARFS_HAVE_FLATBUFFERS) && defined(DWARFS_HAVE_THRIFT)
      override
#endif
      ;
  
  bool empty() const
#if defined(DWARFS_HAVE_FLATBUFFERS) && defined(DWARFS_HAVE_THRIFT)
      override
#endif
      ;
  
  std::shared_ptr<chunk_view_interface const> at(size_t index) const
#if defined(DWARFS_HAVE_FLATBUFFERS) && defined(DWARFS_HAVE_THRIFT)
      override
#endif
      ;

#if defined(DWARFS_HAVE_FLATBUFFERS) && defined(DWARFS_HAVE_THRIFT)
  // Dual-format: provide both backend-specific and interface iterators
  iterator native_begin() const;
  iterator native_end() const;
#else
  // Single-format: only backend-specific iterators
  iterator begin() const;
  iterator end() const;
#endif

#if defined(DWARFS_HAVE_FLATBUFFERS) && defined(DWARFS_HAVE_THRIFT)
  // Iterator implementation for interface (type-erased) - only in dual-format builds
  class iterator_impl : public chunk_range_interface::iterator_interface {
   public:
    iterator_impl(flatbuffer_chunk_range const* range, size_t index)
        : range_(range), index_(index) {}

    std::shared_ptr<chunk_view_interface const> get() const override {
      return range_ ? range_->at(index_) : nullptr;
    }

    void increment() override {
      ++index_;
    }

    bool equal(chunk_range_interface::iterator_interface const& other) const override {
      auto const* other_impl = dynamic_cast<iterator_impl const*>(&other);
      return other_impl && range_ == other_impl->range_ && index_ == other_impl->index_;
    }

    std::unique_ptr<chunk_range_interface::iterator_interface> clone() const override {
      return std::make_unique<iterator_impl>(range_, index_);
    }

   private:
    flatbuffer_chunk_range const* range_;
    size_t index_;
  };

  // Virtual override for interface (only in dual-format)
  chunk_range_interface::iterator begin() const override;
  chunk_range_interface::iterator end() const override;
#endif

 private:
  ::dwarfs::flatbuffers::Metadata const* metadata_;
  uint32_t begin_index_;
  uint32_t end_index_;
};

/**
 * Iterator for flatbuffer_chunk_range
 * Provides compatibility with boost::iterator_facade pattern
 */
class flatbuffer_chunk_range::iterator {
 public:
  using iterator_category = std::random_access_iterator_tag;
  using value_type = flatbuffer_chunk_view;
  using difference_type = std::ptrdiff_t;
  using pointer = flatbuffer_chunk_view const*;
  using reference = flatbuffer_chunk_view const&;

  iterator() = default;
  iterator(flatbuffer_chunk_range const* range, size_t index);

  reference operator*() const;
  pointer operator->() const;

  iterator& operator++();
  iterator operator++(int);
  iterator& operator--();
  iterator operator--(int);

  iterator& operator+=(difference_type n);
  iterator& operator-=(difference_type n);

  iterator operator+(difference_type n) const;
  iterator operator-(difference_type n) const;
  difference_type operator-(iterator const& other) const;

  bool operator==(iterator const& other) const;
  bool operator!=(iterator const& other) const;
  bool operator<(iterator const& other) const;
  bool operator<=(iterator const& other) const;
  bool operator>(iterator const& other) const;
  bool operator>=(iterator const& other) const;

 private:
  flatbuffer_chunk_range const* range_ = nullptr;
  size_t index_ = 0;
  mutable std::shared_ptr<flatbuffer_chunk_view const> cached_view_;
};

/**
 * FlatBuffer-based inode view
 */
class flatbuffer_inode_view : public inode_view_interface {
 public:
  flatbuffer_inode_view(::dwarfs::flatbuffers::InodeData const* inode_data,
                        uint32_t inode_num,
                        ::dwarfs::flatbuffers::Metadata const* metadata);

  mode_type mode() const override;
  std::string mode_string() const override;
  std::string perm_string() const override;
  posix_file_type::value type() const override;
  uid_type getuid() const override;
  gid_type getgid() const override;
  uint32_t inode_num() const override;
  bool is_directory() const override;

 private:
  ::dwarfs::flatbuffers::InodeData const* inode_data_;
  uint32_t inode_num_;
  ::dwarfs::flatbuffers::Metadata const* metadata_;
};

/**
 * FlatBuffer-based directory entry view
 */
class flatbuffer_dir_entry_view : public dir_entry_view_interface {
 public:
  flatbuffer_dir_entry_view(::dwarfs::flatbuffers::DirEntry const* dir_entry,
                            uint32_t self_index, uint32_t parent_index,
                            ::dwarfs::flatbuffers::Metadata const* metadata);

  std::string name() const override;
  std::shared_ptr<inode_view_interface> inode() const override;
  uint32_t self_index() const override;
  uint32_t parent_index() const override;
  bool is_root() const override;
  std::string path() const override;

 private:
  ::dwarfs::flatbuffers::DirEntry const* dir_entry_;
  uint32_t self_index_;
  uint32_t parent_index_;
  ::dwarfs::flatbuffers::Metadata const* metadata_;

  // Helper to convert entry index to directory index
  uint32_t entry_to_dir_idx(uint32_t entry_idx) const;
};

/**
 * FlatBuffer-based directory view
 */
class flatbuffer_directory_view : public directory_view_interface {
 public:
  flatbuffer_directory_view(::dwarfs::flatbuffers::Directory const* directory,
                            uint32_t inode_num,
                            ::dwarfs::flatbuffers::Metadata const* metadata);

  uint32_t inode() const override;
  size_t entry_count() const override;
  uint32_t first_entry() const override;
  uint32_t parent_entry() const override;
  uint32_t self_entry() const override;

 private:
  ::dwarfs::flatbuffers::Directory const* directory_;
  uint32_t inode_num_;
  ::dwarfs::flatbuffers::Metadata const* metadata_;
};

/**
 * FlatBuffer-based global metadata
 */
class flatbuffer_global_metadata : public global_metadata_interface {
 public:
  explicit flatbuffer_global_metadata(::dwarfs::flatbuffers::Metadata const* metadata);

  std::span<uint8_t const> uids() const override;
  std::span<uint8_t const> gids() const override;
  std::span<uint8_t const> modes() const override;
  std::string name_at(uint32_t index) const override;
  std::string symlink_at(uint32_t index) const override;
  uint32_t block_size() const override;
  uint64_t total_fs_size() const override;
  std::optional<uint32_t> hole_block_index() const override;

 private:
  ::dwarfs::flatbuffers::Metadata const* metadata_;
};

} // namespace dwarfs::reader::internal

#endif // DWARFS_HAVE_FLATBUFFERS