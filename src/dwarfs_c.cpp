/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \author     Marcus Holland-Moritz (github@mhxnet.de)
 * \copyright  Copyright (c) Marcus Holland-Moritz
 *
 * This file is part of dwarfs.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the “Software”), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file dwarfs_c.cpp
 * @brief Implementation of the stable C ABI for the DwarFS reader.
 *
 * Thin layer over the C++ reader (dwarfs::reader::filesystem_v2). No C++
 * exceptions cross the boundary; errors are reported through a thread-local
 * errno-style channel. See include/dwarfs_c.h for the full contract.
 */

#include <dwarfs_c.h>

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <memory>
#include <new>
#include <optional>
#include <span>
#include <string>
#include <system_error>
#include <utility>

#include <dwarfs/detail/file_extent_info.h>
#include <dwarfs/detail/file_segment_impl.h>
#include <dwarfs/detail/file_view_impl.h>
#include <dwarfs/extent_kind.h>
#include <dwarfs/file_extents_iterable.h>
#include <dwarfs/file_range.h>
#include <dwarfs/file_segment.h>
#include <dwarfs/file_stat.h>
#include <dwarfs/file_type.h>
#include <dwarfs/file_view.h>
#include <dwarfs/io_advice.h>
#include <dwarfs/logger.h>
#include <dwarfs/os_access_generic.h>
#include <dwarfs/reader/filesystem_options.h>
#include <dwarfs/reader/filesystem_v2.h>
#include <dwarfs/reader/fsinfo_options.h>
#include <dwarfs/types.h>
#include <dwarfs/version.h>

namespace {

// ---------------------------------------------------------------------------
// Thread-local errno-style error channel
// ---------------------------------------------------------------------------

thread_local int t_last_error{0};
thread_local std::string t_last_message;

void clear_error() {
  t_last_error = 0;
  t_last_message.clear();
}

int fail(int err, std::string msg = {}) {
  t_last_error = err;
  t_last_message = std::move(msg);
  return -1;
}

int fail_from_exception(std::exception const& e) {
  if (auto const* se = dynamic_cast<std::system_error const*>(&e)) {
    auto const& ec = se->code();
    if (ec.category() == std::generic_category()) {
      return fail(ec.value(), e.what());
    }
  }
  if (dynamic_cast<std::invalid_argument const*>(&e)) {
    return fail(EINVAL, e.what());
  }
  if (dynamic_cast<std::bad_alloc const*>(&e)) {
    return fail(ENOMEM, e.what());
  }
  return fail(EIO, e.what());
}

// ---------------------------------------------------------------------------
// Memory-backed file view (borrowing; the caller owns the buffer)
// ---------------------------------------------------------------------------

class memory_file_segment final : public dwarfs::detail::file_segment_impl {
 public:
  memory_file_segment(
      std::shared_ptr<dwarfs::detail::file_view_impl const> parent,
      dwarfs::file_range range, std::byte const* base)
      : parent_{std::move(parent)}
      , range_{range}
      , base_{base} {}

  dwarfs::file_off_t offset() const noexcept override {
    return range_.offset();
  }

  dwarfs::file_size_t size() const noexcept override { return range_.size(); }

  dwarfs::file_range range() const noexcept override { return range_; }

  bool is_zero() const noexcept override { return false; }

  std::span<std::byte const> raw_bytes() const override {
    return {base_ + range_.offset(), static_cast<size_t>(range_.size())};
  }

  void advise(dwarfs::io_advice, std::error_code& ec) const override {
    ec.clear();
  }

  void lock(std::error_code& ec) const override { ec.clear(); }

 private:
  std::shared_ptr<dwarfs::detail::file_view_impl const> parent_;
  dwarfs::file_range range_;
  std::byte const* base_;
};

class memory_file_view final
    : public dwarfs::detail::file_view_impl,
      public std::enable_shared_from_this<memory_file_view> {
 public:
  memory_file_view(void const* data, size_t size)
      : data_{static_cast<std::byte const*>(data)}
      , size_{size} {}

  dwarfs::file_size_t size() const override { return size_; }

  std::filesystem::path const& path() const override { return path_; }

  dwarfs::file_segment segment_at(dwarfs::file_range range) const override {
    if (range.offset() < 0 || range.size() == 0 ||
        std::cmp_greater(range.offset() + range.size(), size_)) {
      return {};
    }
    return dwarfs::file_segment{std::make_shared<memory_file_segment>(
        shared_from_this(), range, data_)};
  }

  dwarfs::file_extents_iterable
  extents(std::optional<dwarfs::file_range> range) const override {
    if (!range) {
      range = dwarfs::file_range{0, size_};
    }
    // extent_ backs the span held by the returned iterable; it stays valid
    // as long as the view is alive.
    extent_ = dwarfs::detail::file_extent_info{dwarfs::extent_kind::data,
                                               *range};
    return dwarfs::file_extents_iterable{
        shared_from_this(), std::span{&extent_, 1}, *range};
  }

  bool supports_raw_bytes() const noexcept override { return true; }

  std::span<std::byte const> raw_bytes() const override {
    return {data_, size_};
  }

  void copy_bytes(void* dest, dwarfs::file_range range,
                  std::error_code& ec) const override {
    if (range.size() == 0) {
      ec.clear();
      return;
    }
    if (!dest || range.offset() < 0) {
      ec = std::make_error_code(std::errc::invalid_argument);
      return;
    }
    if (std::cmp_greater(range.offset() + range.size(), size_)) {
      ec = std::make_error_code(std::errc::result_out_of_range);
      return;
    }
    std::memcpy(dest, data_ + range.offset(),
                static_cast<size_t>(range.size()));
    ec.clear();
  }

  size_t default_segment_size() const override { return 64 * 1024; }

  void release_until(dwarfs::file_off_t, std::error_code& ec) const override {
    ec.clear();
  }

 private:
  std::byte const* data_;
  size_t size_;
  std::filesystem::path path_{"<memory>"};
  mutable dwarfs::detail::file_extent_info extent_;
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

int32_t map_file_type(dwarfs::posix_file_type::value type) {
  switch (type) {
    case dwarfs::posix_file_type::regular:
      return DWARFS_C_FILE_REGULAR;
    case dwarfs::posix_file_type::directory:
      return DWARFS_C_FILE_DIRECTORY;
    case dwarfs::posix_file_type::symlink:
      return DWARFS_C_FILE_SYMLINK;
    default:
      return DWARFS_C_FILE_OTHER;
  }
}

// Normalize a lookup path: strip leading and trailing slashes; the empty
// (or all-slashes) path maps to the root directory.
std::string normalize_path(char const* path) {
  std::string p{path};
  auto const first = p.find_first_not_of('/');
  if (first == std::string::npos) {
    return "/";
  }
  auto const last = p.find_last_not_of('/');
  return p.substr(first, last - first + 1);
}

std::optional<dwarfs::reader::dir_entry_view>
find_entry(dwarfs::reader::filesystem_v2& fs, char const* path) {
  return fs.find(normalize_path(path));
}

} // namespace

// ---------------------------------------------------------------------------
// Opaque handle bodies
// ---------------------------------------------------------------------------

struct dwarfs_c_filesystem {
  dwarfs::null_logger lgr;
  dwarfs::os_access_generic os;
  std::unique_ptr<dwarfs::reader::filesystem_v2> fs;
  dwarfs::file_view view; // keeps memory-backed images alive
};

struct dwarfs_c_dir {
  dwarfs::reader::filesystem_v2* fs{nullptr}; // borrowed
  std::optional<dwarfs::reader::directory_view> view;
  size_t offset{0};
  std::string name;  // storage for the current entry name
  int32_t type{DWARFS_C_FILE_UNKNOWN};
};

// ---------------------------------------------------------------------------
// Error channel
// ---------------------------------------------------------------------------

extern "C" {

DWARFS_C_API
int dwarfs_c_errno(void) { return t_last_error; }

DWARFS_C_API
const char* dwarfs_c_error_message(void) { return t_last_message.c_str(); }

DWARFS_C_API
const char* dwarfs_c_strerror(int err) { return std::strerror(err); }

// ---------------------------------------------------------------------------
// Library information
// ---------------------------------------------------------------------------

DWARFS_C_API
int dwarfs_c_version(void) { return dwarfs::get_dwarfs_library_version(); }

DWARFS_C_API
const char* dwarfs_c_version_string(void) { return dwarfs::DWARFS_GIT_DESC; }

// ---------------------------------------------------------------------------
// Filesystem lifecycle
// ---------------------------------------------------------------------------

DWARFS_C_API
dwarfs_c_filesystem* dwarfs_c_open(const char* path) {
  clear_error();
  if (!path || !*path) {
    fail(EINVAL, "path must not be null or empty");
    return nullptr;
  }
  try {
    auto h = std::make_unique<dwarfs_c_filesystem>();
    h->fs = std::make_unique<dwarfs::reader::filesystem_v2>(
        h->lgr, h->os, std::filesystem::path{path});
    return h.release();
  } catch (std::exception const& e) {
    fail_from_exception(e);
  } catch (...) {
    fail(EIO, "unknown error");
  }
  return nullptr;
}

DWARFS_C_API
dwarfs_c_filesystem*
dwarfs_c_open_region(const char* path, int64_t offset, int64_t length) {
  clear_error();
  static_assert(DWARFS_C_OFFSET_AUTO ==
                dwarfs::reader::filesystem_options::IMAGE_OFFSET_AUTO);
  if (!path || !*path) {
    fail(EINVAL, "path must not be null or empty");
    return nullptr;
  }
  if (length <= 0) {
    fail(EINVAL, "length must be positive");
    return nullptr;
  }
  if (offset < 0 && offset != DWARFS_C_OFFSET_AUTO) {
    fail(EINVAL, "offset must be >= 0 or DWARFS_C_OFFSET_AUTO");
    return nullptr;
  }
  try {
    auto h = std::make_unique<dwarfs_c_filesystem>();
    dwarfs::reader::filesystem_options opts;
    opts.image_offset = offset;
    opts.image_size = length;
    h->fs = std::make_unique<dwarfs::reader::filesystem_v2>(
        h->lgr, h->os, std::filesystem::path{path}, opts);
    return h.release();
  } catch (std::exception const& e) {
    fail_from_exception(e);
  } catch (...) {
    fail(EIO, "unknown error");
  }
  return nullptr;
}

DWARFS_C_API
dwarfs_c_filesystem* dwarfs_c_open_memory(const void* data, size_t size) {
  clear_error();
  if (!data || size == 0) {
    fail(EINVAL, "data must not be null and size must be non-zero");
    return nullptr;
  }
  try {
    auto h = std::make_unique<dwarfs_c_filesystem>();
    h->view =
        dwarfs::file_view{std::make_shared<memory_file_view>(data, size)};
    dwarfs::reader::filesystem_options opts;
    opts.image_offset = 0;
    opts.image_size = static_cast<dwarfs::file_off_t>(size);
    h->fs = std::make_unique<dwarfs::reader::filesystem_v2>(h->lgr, h->os,
                                                            h->view, opts);
    return h.release();
  } catch (std::exception const& e) {
    fail_from_exception(e);
  } catch (...) {
    fail(EIO, "unknown error");
  }
  return nullptr;
}

DWARFS_C_API
void dwarfs_c_close(dwarfs_c_filesystem* fs) { delete fs; }

// ---------------------------------------------------------------------------
// Lookup / stat
// ---------------------------------------------------------------------------

DWARFS_C_API
int dwarfs_c_stat(dwarfs_c_filesystem* fs, const char* path,
                  struct dwarfs_c_stat* st) {
  clear_error();
  if (!fs || !path || !st) {
    return fail(EINVAL, "filesystem, path and stat output must not be null");
  }
  try {
    auto entry = find_entry(*fs->fs, path);
    if (!entry) {
      return fail(ENOENT, "no such file or directory");
    }
    std::error_code ec;
    auto fst = fs->fs->getattr(entry->inode(), ec);
    if (ec) {
      return fail(EIO, "failed to get file attributes: " + ec.message());
    }
    st->size = static_cast<int64_t>(fst.size());
    st->mtime = static_cast<int64_t>(fst.mtime());
    st->mtime_nsec = static_cast<int32_t>(fst.mtime_nsec_unchecked());
    st->mode = static_cast<uint32_t>(fst.mode());
    st->uid = static_cast<uint32_t>(fst.uid());
    st->gid = static_cast<uint32_t>(fst.gid());
    st->nlink = static_cast<uint32_t>(fst.nlink());
    st->type = map_file_type(dwarfs::posix_file_type::from_mode(fst.mode()));
    return 0;
  } catch (std::exception const& e) {
    return fail_from_exception(e);
  } catch (...) {
    return fail(EIO, "unknown error");
  }
}

// ---------------------------------------------------------------------------
// Reading
// ---------------------------------------------------------------------------

DWARFS_C_API
int64_t dwarfs_c_pread(dwarfs_c_filesystem* fs, const char* path, void* buf,
                       size_t count, int64_t offset) {
  clear_error();
  if (!fs || !path || (!buf && count > 0)) {
    return fail(EINVAL, "filesystem, path and buffer must not be null");
  }
  if (offset < 0) {
    return fail(EINVAL, "offset must not be negative");
  }
  try {
    auto entry = find_entry(*fs->fs, path);
    if (!entry) {
      return fail(ENOENT, "no such file or directory");
    }
    auto inode = entry->inode();
    std::error_code ec;
    auto fst = fs->fs->getattr(inode, ec);
    if (ec) {
      return fail(EIO, "failed to get file attributes: " + ec.message());
    }
    auto const type = dwarfs::posix_file_type::from_mode(fst.mode());
    if (type == dwarfs::posix_file_type::directory) {
      return fail(EISDIR, "path is a directory");
    }
    if (type != dwarfs::posix_file_type::regular) {
      return fail(EINVAL, "path is not a regular file");
    }
    auto const size = static_cast<int64_t>(fst.size());
    if (count == 0 || offset >= size) {
      return 0;
    }
    auto const to_read =
        std::min(count, static_cast<size_t>(size - offset));
    size_t const bytes_read = fs->fs->read(
        inode.inode_num(), static_cast<char*>(buf), to_read, offset, ec);
    if (ec) {
      return fail(EIO, "read failed: " + ec.message());
    }
    return static_cast<int64_t>(bytes_read);
  } catch (std::exception const& e) {
    return fail_from_exception(e);
  } catch (...) {
    return fail(EIO, "unknown error");
  }
}

// ---------------------------------------------------------------------------
// Directory listing
// ---------------------------------------------------------------------------

DWARFS_C_API
dwarfs_c_dir* dwarfs_c_opendir(dwarfs_c_filesystem* fs, const char* path) {
  clear_error();
  if (!fs || !path) {
    fail(EINVAL, "filesystem and path must not be null");
    return nullptr;
  }
  try {
    auto entry = find_entry(*fs->fs, path);
    if (!entry) {
      fail(ENOENT, "no such file or directory");
      return nullptr;
    }
    auto inode = entry->inode();
    std::error_code ec;
    auto fst = fs->fs->getattr(inode, ec);
    if (ec) {
      fail(EIO, "failed to get file attributes: " + ec.message());
      return nullptr;
    }
    if (dwarfs::posix_file_type::from_mode(fst.mode()) !=
        dwarfs::posix_file_type::directory) {
      fail(ENOTDIR, "path is not a directory");
      return nullptr;
    }
    auto view = fs->fs->opendir(inode);
    if (!view) {
      fail(ENOTDIR, "path is not a directory");
      return nullptr;
    }
    auto dir = std::make_unique<dwarfs_c_dir>();
    dir->fs = fs->fs.get();
    dir->view = *view;
    return dir.release();
  } catch (std::exception const& e) {
    fail_from_exception(e);
  } catch (...) {
    fail(EIO, "unknown error");
  }
  return nullptr;
}

DWARFS_C_API
int dwarfs_c_readdir(dwarfs_c_dir* dir, dwarfs_c_dirent* out) {
  clear_error();
  if (!dir || !out) {
    return fail(EINVAL, "directory iterator and output must not be null");
  }
  try {
    while (true) {
      auto entry = dir->fs->readdir(*dir->view, dir->offset++);
      if (!entry) {
        return 0;
      }
      std::string name = entry->name();
      if (name.empty() || name == "." || name == "..") {
        continue;
      }
      std::error_code ec;
      auto fst = dir->fs->getattr(entry->inode(), ec);
      if (ec) {
        return fail(EIO, "failed to get file attributes: " + ec.message());
      }
      dir->name = std::move(name);
      dir->type = map_file_type(dwarfs::posix_file_type::from_mode(fst.mode()));
      out->name = dir->name.c_str();
      out->type = dir->type;
      return 1;
    }
  } catch (std::exception const& e) {
    return fail_from_exception(e);
  } catch (...) {
    return fail(EIO, "unknown error");
  }
}

DWARFS_C_API
void dwarfs_c_closedir(dwarfs_c_dir* dir) { delete dir; }

// ---------------------------------------------------------------------------
// Image metadata
// ---------------------------------------------------------------------------

DWARFS_C_API
char* dwarfs_c_image_info_json(dwarfs_c_filesystem* fs) {
  clear_error();
  if (!fs) {
    fail(EINVAL, "filesystem must not be null");
    return nullptr;
  }
  try {
    dwarfs::reader::fsinfo_options opts;
    opts.features |= dwarfs::reader::fsinfo_feature::version;
    opts.features |= dwarfs::reader::fsinfo_feature::history;
    opts.features |= dwarfs::reader::fsinfo_feature::metadata_summary;
    opts.features |= dwarfs::reader::fsinfo_feature::section_details;
    auto const json = fs->fs->info_as_json(opts).dump();
    auto* out = static_cast<char*>(std::malloc(json.size() + 1));
    if (!out) {
      fail(ENOMEM, "out of memory");
      return nullptr;
    }
    std::memcpy(out, json.c_str(), json.size() + 1);
    return out;
  } catch (std::exception const& e) {
    fail_from_exception(e);
  } catch (...) {
    fail(EIO, "unknown error");
  }
  return nullptr;
}

DWARFS_C_API
void dwarfs_c_free(void* ptr) { std::free(ptr); }

} // extern "C"
