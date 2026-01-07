/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * Domain-based metadata view wrappers
 * These wrap metadata::domain::metadata to provide view interface
 */

#pragma once

#include <memory>
#include <dwarfs/metadata/domain/metadata.h>
#include <dwarfs/reader/internal/metadata_view_interface.h>
#include <filesystem>
#include <cwchar>

namespace dwarfs::reader::internal {

// Forward declarations
class domain_global_metadata;
class domain_inode_view_impl;
class domain_dir_entry_view_impl;
class domain_chunk_view;
class domain_chunk_range_impl;

/**
 * Global metadata wrapper for domain model
 */
class domain_global_metadata
#if defined(DWARFS_HAVE_FLATBUFFERS) && defined(DWARFS_HAVE_THRIFT)
    : public global_metadata_interface
#endif
{
 public:
  explicit domain_global_metadata(metadata::domain::metadata const& meta)
      : meta_{meta} {}

#if !defined(DWARFS_HAVE_FLATBUFFERS) || !defined(DWARFS_HAVE_THRIFT)
  // Non-interface methods for single-format builds
  // Create directory entry view
  std::shared_ptr<domain_dir_entry_view_impl const>
  make_dir_entry_view(uint32_t self_index, uint32_t parent_index) const;

  // Get self entry index for a directory inode
  uint32_t self_dir_entry(uint32_t dir_inode) const;

  // Get first entry index for a directory inode
  uint32_t first_dir_entry(uint32_t dir_inode) const;

  // Get parent entry index for a directory inode
  uint32_t parent_dir_entry(uint32_t dir_inode) const;
#else
  // Interface override: Get self entry index for a directory inode
  uint32_t self_dir_entry(uint32_t dir_inode) const;
#endif

  // Accessor for actual metadata (const reference)
  metadata::domain::metadata const& meta() const { return meta_; }

  // Accessor for backend_adapter (needed for thrift-only conversion)
  metadata::domain::metadata const& domain_meta() const { return meta_; }

#if defined(DWARFS_HAVE_FLATBUFFERS) && defined(DWARFS_HAVE_THRIFT)
  // Implement global_metadata_interface for dual-format builds
  std::span<uint8_t const> uids() const override;
  std::span<uint8_t const> gids() const override;
  std::span<uint8_t const> modes() const override;
  std::string name_at(uint32_t index) const override;
  std::string symlink_at(uint32_t index) const override;
  uint32_t block_size() const override;
  uint64_t total_fs_size() const override;
  std::optional<uint32_t> hole_block_index() const override;
  uint32_t first_dir_entry(uint32_t ino) const override;
  uint32_t parent_dir_entry(uint32_t ino) const override;
  std::shared_ptr<dir_entry_view_interface const>
  make_dir_entry_view(uint32_t index, uint32_t parent_index) const override;
  std::shared_ptr<dir_entry_view_interface const>
  make_dir_entry_view(uint32_t index) const override;
#endif

 private:
  metadata::domain::metadata const& meta_;
};

/**
 * Inode view implementation for domain model
 */
class domain_inode_view_impl : public inode_view_interface {
 public:
  domain_inode_view_impl(metadata::domain::metadata const& meta,
                         uint32_t inode_index, uint32_t inode_num);

  mode_type mode() const override;
  std::string mode_string() const override;
  std::string perm_string() const override;
  posix_file_type::value type() const override;
  uid_type getuid() const override;
  gid_type getgid() const override;
  uint32_t inode_num() const override;
  bool is_directory() const override;

  // Accessors for backend_adapter (needed for thrift-only conversion)
  uint32_t inode_index() const { return inode_index_; }
  metadata::domain::metadata const& domain_meta() const { return meta_; }

 private:
  metadata::domain::metadata const& meta_;
  uint32_t inode_index_;
  uint32_t inode_num_;
};

/**
 * Directory entry view implementation for domain model
 */
class domain_dir_entry_view_impl : public dir_entry_view_interface {
 public:
  domain_dir_entry_view_impl(metadata::domain::metadata const& meta,
                             uint32_t self_index, uint32_t parent_index);

  std::string name() const override;
  std::shared_ptr<inode_view_interface> inode() const override;
  std::shared_ptr<inode_view_interface const> inode_shared() const override;
  uint32_t self_index() const override;
  uint32_t parent_index() const override;
  bool is_root() const override;
  std::string path() const override;
  std::string unix_path() const override;
  std::filesystem::path fs_path() const override;
  std::wstring wpath() const override;
  std::unique_ptr<dir_entry_view_interface> parent() const override;

  // Accessor for backend_adapter (needed for thrift-only conversion)
  metadata::domain::metadata const& domain_meta() const { return meta_; }

 private:
  metadata::domain::metadata const& meta_;
  uint32_t self_index_;
  uint32_t parent_index_;
};

/**
 * Chunk view implementation for domain model
 */
class domain_chunk_view : public chunk_view_interface {
 public:
  domain_chunk_view(metadata::domain::metadata const& meta, uint32_t chunk_index);

  uint32_t block() const override;
  uint32_t offset() const override;
  file_off_t size() const override;
  bool is_data() const override;
  bool is_hole() const override;

 private:
  metadata::domain::metadata const& meta_;
  uint32_t chunk_index_;
};

/**
 * Chunk range implementation for domain model
 */
class domain_chunk_range_impl
#if defined(DWARFS_HAVE_FLATBUFFERS) && defined(DWARFS_HAVE_THRIFT)
    : public chunk_range_interface
#endif
{
 public:
#if defined(DWARFS_HAVE_FLATBUFFERS) && defined(DWARFS_HAVE_THRIFT)
  // Type-erased iterator implementation for dual-format builds
  class iterator_impl : public chunk_range_interface::iterator_interface {
   public:
    iterator_impl(metadata::domain::metadata const& meta, uint32_t index)
        : meta_{&meta}, index_{index} {}

    std::shared_ptr<chunk_view_interface const> get() const override {
      return std::make_shared<domain_chunk_view>(*meta_, index_);
    }

    void increment() override {
      ++index_;
    }

    bool equal(chunk_range_interface::iterator_interface const& other) const override {
      auto const* other_impl = dynamic_cast<iterator_impl const*>(&other);
      return other_impl && index_ == other_impl->index_;
    }

    std::unique_ptr<chunk_range_interface::iterator_interface> clone() const override {
      return std::make_unique<iterator_impl>(*meta_, index_);
    }

   private:
    metadata::domain::metadata const* meta_{nullptr};
    uint32_t index_{0};
  };
#endif

  // Iterator class for chunk range (used in FlatBuffers-only and Thrift-only)
  class iterator {
   public:
    using value_type = std::shared_ptr<chunk_view_interface const>;
    using difference_type = std::ptrdiff_t;
    using pointer = value_type const*;
    using reference = value_type const&;
    using iterator_category = std::input_iterator_tag;

    iterator() = default;
    iterator(metadata::domain::metadata const& meta, uint32_t index)
        : meta_{&meta}, index_{index} {}

    value_type operator*() const {
      return std::make_shared<domain_chunk_view>(*meta_, index_);
    }

    value_type operator->() const { return **this; }

    iterator& operator++() {
      ++index_;
      return *this;
    }

    iterator operator++(int) {
      auto tmp = *this;
      ++(*this);
      return tmp;
    }

    bool operator==(iterator const& other) const {
      return index_ == other.index_;
    }

    bool operator!=(iterator const& other) const { return !(*this == other); }

   private:
    metadata::domain::metadata const* meta_{nullptr};
    uint32_t index_{0};
  };

  domain_chunk_range_impl(metadata::domain::metadata const& meta,
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
  // Dual-format: return interface iterator
  chunk_range_interface::iterator begin() const override {
    return chunk_range_interface::iterator{
        std::make_unique<iterator_impl>(meta_, begin_index_)};
  }

  chunk_range_interface::iterator end() const override {
    return chunk_range_interface::iterator{
        std::make_unique<iterator_impl>(meta_, end_index_)};
  }
#else
  // Single-format: return native iterator
  iterator begin() const { return iterator{meta_, begin_index_}; }
  iterator end() const { return iterator{meta_, end_index_}; }
#endif

 private:
  metadata::domain::metadata const& meta_;
  uint32_t begin_index_;
  uint32_t end_index_;
};

} // namespace dwarfs::reader::internal