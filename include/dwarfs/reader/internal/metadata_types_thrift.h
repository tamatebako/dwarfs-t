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

#ifdef DWARFS_HAVE_THRIFT

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <variant>
#include <cstddef>
#include <memory>
#include <cassert>
#include <cstdint>
#include <string>
#include <iomanip>
#include <utility>

#include <dwarfs/file_stat.h>
#include <dwarfs/file_type.h>
#include <dwarfs/metadata_defs.h>
#include <dwarfs/types.h>
#include <dwarfs/fstypes.h>
#include <dwarfs/file_type.h>

#include <dwarfs/internal/packed_ptr.h>
#include <dwarfs/internal/string_table.h>

#include <thrift/lib/cpp2/frozen/FrozenUtil.h>
#include <dwarfs/gen-cpp2/metadata_layouts.h>
#include <dwarfs/gen-cpp2/metadata_types_custom_protocol.h>

#include <dwarfs/reader/internal/metadata_view_interface.h>

namespace dwarfs {
class logger;
}

namespace dwarfs::reader::internal {

// Forward declaration
class metadata_v2_data;

namespace thrift_backend {

/**
 * Thrift-specific global metadata
 *
 * This is the Thrift backend implementation, completely separate from FlatBuffers.
 * All types in this namespace work ONLY with Thrift Frozen2 format.
 */
class global_metadata : public global_metadata_interface {
 public:
  using Meta = ::apache::thrift::frozen::MappedFrozen<
      ::dwarfs::thrift::metadata::metadata>;
  using directories_view = ::apache::thrift::frozen::View<
      std::vector<::dwarfs::thrift::metadata::directory>>;
  using bundled_directories_view =
      ::apache::thrift::frozen::Bundled<directories_view>;

  global_metadata(logger& lgr, Meta const& meta);

  static void check_consistency(logger& lgr, Meta const& meta);
  void check_consistency(logger& lgr) const;

  // Interface implementations (global_metadata_interface)
  std::span<uint8_t const> uids() const override;
  std::span<uint8_t const> gids() const override;
  std::span<uint8_t const> modes() const override;
  std::string name_at(uint32_t index) const override;
  std::string symlink_at(uint32_t index) const override;
  uint32_t block_size() const override;
  uint64_t total_fs_size() const override;
  std::optional<uint32_t> hole_block_index() const override;

  // Helper methods (not part of interface)
  Meta const& meta() const { return meta_; }

  uint32_t first_dir_entry(uint32_t ino) const override;
  uint32_t parent_dir_entry(uint32_t ino) const override;
  uint32_t self_dir_entry(uint32_t ino) const override;
  
  std::shared_ptr<dir_entry_view_interface const>
  make_dir_entry_view(uint32_t index, uint32_t parent_index) const override;
  
  std::shared_ptr<dir_entry_view_interface const>
  make_dir_entry_view(uint32_t index) const override;

  dwarfs::internal::string_table const& names() const { return names_; }

  // Thrift may have bundled directories if packed
  std::optional<directories_view> bundled_directories() const;

 private:
  Meta const& meta_;
  std::optional<bundled_directories_view> const bundled_directories_;
  directories_view const directories_;
  dwarfs::internal::string_table const names_;
  // Materialized data for efficient span access
  std::vector<uint32_t> const materialized_uids_;
  std::vector<uint32_t> const materialized_gids_;
  std::vector<uint16_t> const materialized_modes_;
};

/**
 * Thrift-specific inode view implementation
 */
class inode_view_impl : public inode_view_interface {
 public:
  using uid_type = file_stat::uid_type;
  using gid_type = file_stat::gid_type;
  using mode_type = file_stat::mode_type;
  using InodeView = ::apache::thrift::frozen::View<
      ::dwarfs::thrift::metadata::inode_data>;

  inode_view_impl(InodeView inode_data, uint32_t inode_num,
                  global_metadata::Meta const& meta)
      : inode_data_{inode_data}
      , inode_num_{inode_num}
      , meta_{&meta} {}

  mode_type mode() const override;
  std::string mode_string() const override;
  std::string perm_string() const override;
  posix_file_type::value type() const override { return posix_file_type::from_mode(mode()); }
  uid_type getuid() const override;
  gid_type getgid() const override;
  uint32_t inode_num() const override { return inode_num_; }
  bool is_directory() const override { return type() == posix_file_type::directory; }

  // Additional methods needed by metadata_v2_data
  uint32_t mode_index() const { return inode_data_.mode_index(); }
  uint32_t owner_index() const { return inode_data_.owner_index(); }
  uint32_t group_index() const { return inode_data_.group_index(); }
  uint32_t nlink_minus_one() const {
    return inode_data_.nlink_minus_one();
  }
  uint32_t inode_v2_2() const { return inode_data_.inode_v2_2(); }
  uint32_t name_index_v2_2() const {
    return inode_data_.name_index_v2_2();
  }

  // Timestamp accessors for time_resolution_handler
  uint64_t mtime_offset() const {
    return inode_data_.mtime_offset();
  }
  uint64_t atime_offset() const {
    return inode_data_.atime_offset();
  }
  uint64_t ctime_offset() const {
    return inode_data_.ctime_offset();
  }
  uint64_t btime_offset() const {
    return inode_data_.btime_offset();
  }
  uint64_t mtime_subsec() const {
    return inode_data_.mtime_subsec();
  }
  uint64_t atime_subsec() const {
    return inode_data_.atime_subsec();
  }
  uint64_t ctime_subsec() const {
    return inode_data_.ctime_subsec();
  }
  uint64_t btime_subsec() const {
    return inode_data_.btime_subsec();
  }

 private:
  InodeView inode_data_;
  uint32_t inode_num_;
  global_metadata::Meta const* meta_;
};

/**
 * Thrift-specific directory entry view implementation
 */
class dir_entry_view_impl : public dir_entry_view_interface {
 public:
  enum class entry_name_type : uint8_t {
    other,
    self,
    parent,
  };

  using DirEntryView =
      ::apache::thrift::frozen::View<::dwarfs::thrift::metadata::dir_entry>;
  using InodeView = ::apache::thrift::frozen::View<
      ::dwarfs::thrift::metadata::inode_data>;

  dir_entry_view_impl(DirEntryView dir_entry, uint32_t self_index,
                      uint32_t parent_index, global_metadata const& g,
                      entry_name_type name_type)
      : v_{dir_entry}
      , self_index_{self_index}
      , parent_index_{parent_index}
      , g_{&g, name_type} {}

  dir_entry_view_impl(InodeView inode_data, uint32_t self_index,
                      uint32_t parent_index, global_metadata const& g,
                      entry_name_type name_type)
      : v_{inode_data}
      , self_index_{self_index}
      , parent_index_{parent_index}
      , g_{&g, name_type} {}

  static std::shared_ptr<dir_entry_view_impl> from_dir_entry_index_shared(
      uint32_t self_index, uint32_t parent_index, global_metadata const& g,
      entry_name_type name_type = entry_name_type::other);
  static std::shared_ptr<dir_entry_view_impl> from_dir_entry_index_shared(
      uint32_t self_index, global_metadata const& g,
      entry_name_type name_type = entry_name_type::other);

  static dir_entry_view_impl
  from_dir_entry_index(uint32_t self_index, uint32_t parent_index,
                       global_metadata const& g,
                       entry_name_type name_type = entry_name_type::other);

  static std::string name(uint32_t index, global_metadata const& g);

  std::string name() const override;
  std::shared_ptr<inode_view_interface const> inode_shared() const override;
  std::shared_ptr<inode_view_interface> inode() const override;
  inode_view_impl inode_concrete() const;

  bool is_root() const override;

  std::shared_ptr<dir_entry_view_impl> parent_shared() const;
  std::unique_ptr<dir_entry_view_interface> parent() const override;

  std::string path() const override;
  std::string unix_path() const override;
  std::filesystem::path fs_path() const override;
  std::wstring wpath() const override;

  void append_to(std::filesystem::path& p) const;

  uint32_t self_index() const override { return self_index_; }
  uint32_t parent_index() const override { return parent_index_; }

 private:
  template <template <typename...> class Ctor>
  auto make_inode() const;

  template <template <typename...> class Ctor>
  static auto
  make_dir_entry_view(uint32_t self_index, uint32_t parent_index,
                      global_metadata const& g, entry_name_type name_type);

  template <template <typename...> class Ctor>
  static auto make_dir_entry_view(uint32_t self_index, global_metadata const& g,
                                  entry_name_type name_type);

  std::variant<DirEntryView, InodeView> v_;
  uint32_t self_index_, parent_index_;
  dwarfs::internal::packed_ptr<global_metadata const, 2, entry_name_type> const
      g_;
};

/**
 * Thrift-specific chunk view
 */
class chunk_view : public chunk_view_interface {
  using ChunkView =
      ::apache::thrift::frozen::View<::dwarfs::thrift::metadata::chunk>;

 public:
  chunk_view() = default;
  explicit chunk_view(ChunkView chunk);

  bool is_data() const override { return (bits_ & kChunkBitsHoleBit) == 0; }
  bool is_hole() const override {
    return (bits_ & kChunkBitsHoleBit) == kChunkBitsHoleBit;
  }

  uint32_t block() const override {
    assert(is_data());
    return block_;
  }

  uint32_t offset() const override {
    assert(is_data());
    return offset_;
  }

  file_off_t size() const override { return bits_ & kChunkBitsSizeMask; }

 private:
  uint32_t block_{0};
  uint32_t offset_{0};
  uint64_t bits_{0};
};

/**
 * Thrift-specific chunk range
 */
class chunk_range
#if defined(DWARFS_HAVE_FLATBUFFERS) && defined(DWARFS_HAVE_THRIFT)
    : public chunk_range_interface  // Only inherit in dual-format builds
#endif
{
  using Meta = global_metadata::Meta;
  using ChunksView = ::apache::thrift::frozen::View<
      std::vector<::dwarfs::thrift::metadata::chunk>>;

  friend class dwarfs::reader::internal::metadata_v2_data;

 public:
  class iterator {
   public:
    using iterator_category = std::random_access_iterator_tag;
    using value_type = chunk_view;
    using difference_type = std::ptrdiff_t;
    using pointer = chunk_view const*;
    using reference = chunk_view const&;

    iterator() = default;
    iterator(ChunksView chunks, uint32_t it) : chunks_{chunks}, it_{it} {}

    bool operator==(iterator const& other) const {
      return it_ == other.it_;
    }
    bool operator!=(iterator const& other) const { return !(*this == other); }

    iterator& operator++() {
      ++it_;
      return *this;
    }
    iterator operator++(int) {
      iterator tmp = *this;
      ++(*this);
      return tmp;
    }
    iterator& operator--() {
      --it_;
      return *this;
    }
    iterator operator--(int) {
      iterator tmp = *this;
      --(*this);
      return tmp;
    }

    iterator& operator+=(difference_type n) {
      it_ += n;
      return *this;
    }
    iterator& operator-=(difference_type n) {
      it_ -= n;
      return *this;
    }
    iterator operator+(difference_type n) const {
      return iterator(chunks_, it_ + n);
    }
    iterator operator-(difference_type n) const {
      return iterator(chunks_, it_ - n);
    }
    difference_type operator-(iterator const& other) const {
      return static_cast<difference_type>(it_) -
             static_cast<difference_type>(other.it_);
    }

    reference operator*() const;
    pointer operator->() const;

   private:
    ChunksView chunks_;
    uint32_t it_{0};
    mutable chunk_view view_;
  };

#if defined(DWARFS_HAVE_FLATBUFFERS) && defined(DWARFS_HAVE_THRIFT)
  // Dual-format: provide both backend-specific and interface iterators
  iterator native_begin() const { return {chunks_, begin_}; }
  iterator native_end() const { return {chunks_, end_}; }
#else
  // Single-format: only backend-specific iterators
  iterator begin() const { return {chunks_, begin_}; }
  iterator end() const { return {chunks_, end_}; }
#endif

  size_t size() const
#if defined(DWARFS_HAVE_FLATBUFFERS) && defined(DWARFS_HAVE_THRIFT)
      override
#endif
  { return end_ - begin_; }
  
  bool empty() const
#if defined(DWARFS_HAVE_FLATBUFFERS) && defined(DWARFS_HAVE_THRIFT)
      override
#endif
  { return end_ == begin_; }
  
  std::shared_ptr<chunk_view_interface const> at(size_t index) const
#if defined(DWARFS_HAVE_FLATBUFFERS) && defined(DWARFS_HAVE_THRIFT)
      override
#endif
  {
    if (index >= size()) {
      return nullptr;
    }
#if defined(DWARFS_HAVE_FLATBUFFERS) && defined(DWARFS_HAVE_THRIFT)
    // Dual-format: use native iterator for performance
    auto it = native_begin() + index;
#else
    // Single-format: use begin() directly
    auto it = begin() + index;
#endif
    return std::make_shared<chunk_view>(*it);
  }

#if defined(DWARFS_HAVE_FLATBUFFERS) && defined(DWARFS_HAVE_THRIFT)
  // Iterator implementation for interface (type-erased) - only in dual-format builds
  class iterator_impl : public chunk_range_interface::iterator_interface {
   public:
    iterator_impl(ChunksView chunks, uint32_t it)
        : chunks_(chunks), it_(it) {}

    std::shared_ptr<chunk_view_interface const> get() const override {
      if (it_ < chunks_.size()) {
        chunk_view view(chunks_[it_]);
        return std::make_shared<chunk_view>(view);
      }
      return nullptr;
    }

    void increment() override {
      ++it_;
    }

    bool equal(chunk_range_interface::iterator_interface const& other) const override {
      auto const* other_impl = dynamic_cast<iterator_impl const*>(&other);
      return other_impl && it_ == other_impl->it_;
    }

    std::unique_ptr<chunk_range_interface::iterator_interface> clone() const override {
      return std::make_unique<iterator_impl>(chunks_, it_);
    }

   private:
    ChunksView chunks_;
    uint32_t it_;
  };

  // Virtual override for interface (only in dual-format)
  chunk_range_interface::iterator begin() const override {
    return chunk_range_interface::iterator{
        std::make_unique<iterator_impl>(chunks_, begin_)};
  }

  chunk_range_interface::iterator end() const override {
    return chunk_range_interface::iterator{
        std::make_unique<iterator_impl>(chunks_, end_)};
  }
#endif

 private:
  chunk_range() = default;

  chunk_range(Meta const& meta, uint32_t begin, uint32_t end)
      : chunks_(meta.chunks())
      , begin_(begin)
      , end_(end) {}

  ChunksView chunks_;
  uint32_t begin_{0};
  uint32_t end_{0};
};

} // namespace thrift_backend
} // namespace dwarfs::reader::internal

#endif // DWARFS_HAVE_THRIFT
