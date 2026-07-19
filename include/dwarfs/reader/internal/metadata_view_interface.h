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

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <string_view>

#include <dwarfs/file_stat.h>
#include <dwarfs/file_type.h>
#include <dwarfs/types.h>

namespace dwarfs::reader::internal {

/**
 * Abstract chunk view interface
 *
 * Provides access to chunk data (file content segments) independent of
 * the underlying serialization format (Thrift, FlatBuffers, etc.).
 *
 * A chunk represents either:
 * - Data: A segment of compressed file data (block + offset + size)
 * - Hole: A sparse file hole (size only, no actual data)
 */
class chunk_view_interface {
 public:
  virtual ~chunk_view_interface() = default;

  /**
   * Block number containing this chunk's data
   * Only valid for data chunks (use is_data() to check)
   */
  virtual uint32_t block() const = 0;

  /**
   * Offset within the block where this chunk starts
   * Only valid for data chunks (use is_data() to check)
   */
  virtual uint32_t offset() const = 0;

  /**
   * Size of the chunk (bytes)
   * Valid for both data and hole chunks
   */
  virtual file_off_t size() const = 0;

  /**
   * Check if this is a data chunk (has actual compressed data)
   */
  virtual bool is_data() const = 0;

  /**
   * Check if this is a hole chunk (sparse file hole)
   */
  virtual bool is_hole() const = 0;
};

/**
 * Abstract chunk range interface
 *
 * Provides iteration over a range of chunks (file content).
 * Implementations hide the backend-specific details.
 */
class chunk_range_interface {
 public:
  virtual ~chunk_range_interface() = default;

  /**
   * Number of chunks in this range
   */
  virtual size_t size() const = 0;

  /**
   * Check if the range is empty
   */
  virtual bool empty() const = 0;

  /**
   * Get chunk at specific index
   * @param index Index in range [0, size())
   * @return Shared pointer to chunk view
   */
  virtual std::shared_ptr<chunk_view_interface const> at(size_t index) const = 0;

  /**
   * Abstract iterator interface for chunk traversal
   *
   * Type-erased iterator that hides backend-specific implementation.
   * Provides minimal interface for forward iteration.
   */
  class iterator_interface {
   public:
    virtual ~iterator_interface() = default;
    
    /**
     * Get current chunk
     * @return Shared pointer to current chunk view
     */
    virtual std::shared_ptr<chunk_view_interface const> get() const = 0;
    
    /**
     * Advance to next chunk (++it)
     */
    virtual void increment() = 0;
    
    /**
     * Compare with another iterator
     * @param other Iterator to compare with
     * @return true if iterators point to same position
     */
    virtual bool equal(iterator_interface const& other) const = 0;
    
    /**
     * Clone this iterator (for copying)
     * @return New iterator at same position
     */
    virtual std::unique_ptr<iterator_interface> clone() const = 0;
  };

  /**
   * Value-semantic iterator wrapper
   *
   * Provides standard C++ iterator interface while hiding the
   * backend-specific implementation via type erasure.
   */
  class iterator {
   public:
    using iterator_category = std::forward_iterator_tag;
    using value_type = std::shared_ptr<chunk_view_interface const>;
    using difference_type = std::ptrdiff_t;
    using pointer = value_type const*;
    using reference = value_type const&;

    iterator() = default;
    
    explicit iterator(std::unique_ptr<iterator_interface> impl)
        : impl_(std::move(impl)) {}

    // Copy constructor: clone the implementation
    iterator(iterator const& other)
        : impl_(other.impl_ ? other.impl_->clone() : nullptr) {}

    // Copy assignment
    iterator& operator=(iterator const& other) {
      if (this != &other) {
        impl_ = other.impl_ ? other.impl_->clone() : nullptr;
      }
      return *this;
    }

    // Move constructor/assignment (default)
    iterator(iterator&&) noexcept = default;
    iterator& operator=(iterator&&) noexcept = default;

    // Dereference: get current chunk
    value_type operator*() const {
      return impl_ ? impl_->get() : nullptr;
    }

    value_type operator->() const {
      return impl_ ? impl_->get() : nullptr;
    }

    // Pre-increment: advance iterator
    iterator& operator++() {
      if (impl_) {
        impl_->increment();
      }
      return *this;
    }

    // Post-increment
    iterator operator++(int) {
      iterator tmp = *this;
      ++(*this);
      return tmp;
    }

    // Equality comparison
    bool operator==(iterator const& other) const {
      if (!impl_ && !other.impl_) return true;
      if (!impl_ || !other.impl_) return false;
      return impl_->equal(*other.impl_);
    }

    bool operator!=(iterator const& other) const {
      return !(*this == other);
    }

   private:
    std::unique_ptr<iterator_interface> impl_;
  };

  /**
   * Get iterator to first chunk
   * @return Iterator positioned at first chunk
   */
  virtual iterator begin() const = 0;

  /**
   * Get iterator past last chunk
   * @return End iterator
   */
  virtual iterator end() const = 0;
};

/**
 * Abstract inode view interface
 *
 * Provides access to inode metadata independent of serialization format.
 * An inode represents a filesystem object (file, directory, symlink, etc.).
 */
class inode_view_interface {
 public:
  virtual ~inode_view_interface() = default;

  using uid_type = file_stat::uid_type;
  using gid_type = file_stat::gid_type;
  using mode_type = file_stat::mode_type;

  /**
   * Get the inode's mode (type + permissions)
   */
  virtual mode_type mode() const = 0;

  /**
   * Get the inode's type as string (e.g., "drwxr-xr-x")
   */
  virtual std::string mode_string() const = 0;

  /**
   * Get permission bits as string (e.g., "rwxr-xr-x")
   */
  virtual std::string perm_string() const = 0;

  /**
   * Get the owner user ID
   */
  virtual uid_type getuid() const = 0;

  /**
   * Get the owner group ID
   */
  virtual gid_type getgid() const = 0;

  /**
   * Get the inode number
   */
  virtual uint32_t inode_num() const = 0;

  /**
   * Check if this inode represents a directory
   */
  virtual bool is_directory() const = 0;

  /**
   * Get the POSIX file type (regular, directory, symlink, etc.)
   */
  virtual posix_file_type::value type() const = 0;
};

/**
 * Abstract directory entry view interface
 *
 * Provides access to a directory entry (name + inode) independent of
 * serialization format.
 */
class dir_entry_view_interface {
 public:
  virtual ~dir_entry_view_interface() = default;

  /**
   * Get the entry's name
   */
  virtual std::string name() const = 0;

  /**
   * Get the inode associated with this entry
   */
  virtual std::shared_ptr<inode_view_interface> inode() const = 0;

  /**
   * Get the entry's index within its parent directory
   */
  virtual uint32_t self_index() const = 0;

  /**
   * Get the parent directory entry's index
   */
  virtual uint32_t parent_index() const = 0;

  /**
   * Check if this is the root directory entry
   */
  virtual bool is_root() const = 0;

  /**
   * Get full filesystem path for this entry
   */
  virtual std::string path() const = 0;

  /**
   * Get Unix-style path (forward slashes)
   */
  virtual std::string unix_path() const = 0;

  /**
   * Get filesystem path as std::filesystem::path
   */
  virtual std::filesystem::path fs_path() const = 0;

  /**
   * Get wide-character path (for Windows compatibility)
   */
  virtual std::wstring wpath() const = 0;

  /**
   * Get the inode view as shared pointer
   * Returns the inode associated with this entry
   */
  virtual std::shared_ptr<inode_view_interface const> inode_shared() const = 0;

  /**
   * Get the parent directory entry
   * @return Parent entry, or nullptr if this is root
   */
  virtual std::unique_ptr<dir_entry_view_interface> parent() const = 0;
};

/**
 * Abstract directory view interface
 *
 * Provides access to directory contents independent of serialization format.
 */
class directory_view_interface {
 public:
  virtual ~directory_view_interface() = default;

  /**
   * Get the directory's inode number
   */
  virtual uint32_t inode() const = 0;

  /**
   * Get number of entries in this directory (excluding . and ..)
   */
  virtual size_t entry_count() const = 0;

  /**
   * Get index of first entry in the directory entry table
   */
  virtual uint32_t first_entry() const = 0;

  /**
   * Get parent directory entry index
   */
  virtual uint32_t parent_entry() const = 0;

  /**
   * Get this directory's own entry index
   */
  virtual uint32_t self_entry() const = 0;
};

/**
 * Abstract global metadata interface
 *
 * Provides access to global metadata tables (UIDs, GIDs, modes, strings, etc.)
 * independent of serialization format.
 */
class global_metadata_interface {
 public:
  virtual ~global_metadata_interface() = default;

  /**
   * Get raw UID table data
   */
  virtual std::span<uint8_t const> uids() const = 0;

  /**
   * Get raw GID table data
   */
  virtual std::span<uint8_t const> gids() const = 0;

  /**
   * Get raw mode table data
   */
  virtual std::span<uint8_t const> modes() const = 0;

  /**
   * Get string table for names
   */
  virtual std::string name_at(uint32_t index) const = 0;

  /**
   * Get string table for symlinks
   */
  virtual std::string symlink_at(uint32_t index) const = 0;

  /**
   * Get block size
   */
  virtual uint32_t block_size() const = 0;

  /**
   * Get total filesystem size
   */
  virtual uint64_t total_fs_size() const = 0;

  /**
   * Get optional hole block index (for sparse files)
   */
  virtual std::optional<uint32_t> hole_block_index() const = 0;

  /**
   * Get first directory entry index for an inode
   * @param ino Inode number
   * @return Index of first entry in directory
   */
  virtual uint32_t first_dir_entry(uint32_t ino) const = 0;

  /**
   * Get parent directory entry index for an inode
   * @param ino Inode number
   * @return Parent directory entry index
   */
  virtual uint32_t parent_dir_entry(uint32_t ino) const = 0;

  /**
   * Get self directory entry index for an inode
   * @param ino Inode number
   * @return Self directory entry index
   */
  virtual uint32_t self_dir_entry(uint32_t ino) const = 0;

  /**
   * Create dir_entry_view from directory entry index (factory method)
   * @param index Directory entry index
   * @param parent_index Parent directory entry index
   * @return Shared pointer to directory entry
   */
  virtual std::shared_ptr<dir_entry_view_interface const>
  make_dir_entry_view(uint32_t index, uint32_t parent_index) const = 0;

  /**
   * Create dir_entry_view from directory entry index (2-parameter overload)
   * @param index Directory entry index
   * @return Shared pointer to directory entry
   */
  virtual std::shared_ptr<dir_entry_view_interface const>
  make_dir_entry_view(uint32_t index) const = 0;
};

} // namespace dwarfs::reader::internal