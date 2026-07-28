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
#include <mutex>
#include <new>
#include <optional>
#include <span>
#include <string>
#include <system_error>
#include <utility>

#include <openssl/crypto.h>

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
// One-time native library initialization
// ---------------------------------------------------------------------------

// Initialize OpenSSL exactly once with OPENSSL_INIT_NO_ATEXIT. OpenSSL's
// default is to register OPENSSL_cleanup via atexit(3) on first use, and
// that teardown (ossl_method_store_free → alg_cleanup → OPENSSL_sk_pop_free)
// races any thread still doing EVP work at process exit — a free-under-
// live-use abort seen as SIGABRT/SIGSEGV, "double free or corruption" on
// glibc, or a hang when teardown waits on a lock held by a dying thread.
// It only ever fires under concurrent use at exit (a parallel test harness
// exposes it at ~50% per run on ubuntu; valgrind and serialized runs are
// always clean). Suppressing the atexit registration makes the process
// leak the method store at exit instead — the process is dying anyway,
// which is the same trade curl and other embedders make. Must run before
// the first EVP call: init options take effect only at first init.
void ensure_native_init() {
  static std::once_flag flag;
  std::call_once(
      flag, [] { OPENSSL_init_crypto(OPENSSL_INIT_NO_ATEXIT, nullptr); });
}

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
#ifdef _WIN32
    // MSVC's std::system_category carries raw WinAPI error codes — map
    // the common ones so the C ABI's errno contract holds on Windows
    // (otherwise every filesystem error collapses to EIO).
    if (ec.category() == std::system_category()) {
      switch (ec.value()) {
        case 2:   // ERROR_FILE_NOT_FOUND
        case 3:   // ERROR_PATH_NOT_FOUND (POSIX open() gives ENOENT too)
          return fail(ENOENT, e.what());
        case 5:   // ERROR_ACCESS_DENIED
          return fail(EACCES, e.what());
        case 8:   // ERROR_NOT_ENOUGH_MEMORY
          return fail(ENOMEM, e.what());
        case 32:  // ERROR_SHARING_VIOLATION
          return fail(EBUSY, e.what());
        case 80:  // ERROR_FILE_EXISTS
          return fail(EEXIST, e.what());
        case 87:  // ERROR_INVALID_PARAMETER
          return fail(EINVAL, e.what());
        case 145: // ERROR_DIR_NOT_EMPTY
          return fail(ENOTEMPTY, e.what());
        default:
          break;
      }
    }
#endif
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
  ensure_native_init();
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
  ensure_native_init();
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
  ensure_native_init();
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

// ---------------------------------------------------------------------------
// Image writer (v1)
// ---------------------------------------------------------------------------

#include <dwarfs/block_compressor_parser.h>
#include <dwarfs/thread_pool.h>
#include <dwarfs/writer/categorizer.h>
#include <dwarfs/writer/entry_factory.h>
#include <dwarfs/writer/filesystem_writer.h>
#include <dwarfs/writer/filesystem_writer_options.h>
#include <dwarfs/writer/fragment_order_options.h>
#include <dwarfs/writer/scanner.h>
#include <dwarfs/writer/scanner_options.h>
#include <dwarfs/writer/segmenter_factory.h>
#include <dwarfs/writer/writer_progress.h>

#include <fmt/format.h>

#include <boost/program_options/variables_map.hpp>

#include <chrono>
#include <fstream>
#include <thread>
#include <vector>

namespace {

// mkdwarfs level-7 (default) profile, mirrored so the binding produces
// what plain `mkdwarfs -i dir -o img` produces (see
// tools/src/mkdwarfs/argtable3_options_parser.cpp: levels[7]).
constexpr unsigned kDefaultBlockSizeBits{24};
constexpr char kSchemaCompression[] = "zstd:level=16";
constexpr char kMetadataCompression[] = "zstd:level=22";
constexpr unsigned kWindowSize{12};
constexpr unsigned kWindowStep{3};
constexpr size_t kMaxActiveBlocks{1};
constexpr unsigned kBloomFilterSize{4};

std::string block_compression_spec(dwarfs_c_writer_options const& opts) {
  switch (opts.compression) {
  case DWARFS_C_COMPRESSION_NONE:
    return "null";
  case DWARFS_C_COMPRESSION_LZMA:
    return opts.compression_level < 0
               ? "lzma"
               : fmt::format("lzma:level={}", opts.compression_level);
  case DWARFS_C_COMPRESSION_BROTLI:
    return opts.compression_level < 0
               ? "brotli"
               : fmt::format("brotli:quality={}", opts.compression_level);
  case DWARFS_C_COMPRESSION_ZSTD:
  default:
    // mkdwarfs level-7 data default is zstd:level=22
    return opts.compression_level < 0
               ? "zstd:level=22"
               : fmt::format("zstd:level={}", opts.compression_level);
  }
}

bool compression_level_in_range(int32_t compression, int32_t level) {
  if (level < 0) {
    return true; // -1 = library default
  }
  switch (compression) {
  case DWARFS_C_COMPRESSION_ZSTD:
    return level >= 1 && level <= 22;
  case DWARFS_C_COMPRESSION_LZMA:
    return level >= 0 && level <= 9;
  case DWARFS_C_COMPRESSION_BROTLI:
    return level >= 0 && level <= 11;
  case DWARFS_C_COMPRESSION_NONE:
  default:
    return true; // ignored
  }
}

} // namespace

struct dwarfs_c_writer {
  enum class source_kind { none, tree, files };

  dwarfs::null_logger lgr;
  dwarfs::os_access_generic os;
  dwarfs_c_writer_options opts{};
  source_kind kind{source_kind::none};
  std::filesystem::path tree_path;              // kind == tree
  std::filesystem::path files_root;             // kind == files
  std::vector<std::string> file_names;          // kind == files
  bool written{false};
};

extern "C" {

DWARFS_C_API
void dwarfs_c_writer_options_init(dwarfs_c_writer_options* opts) {
  ensure_native_init();
  if (!opts) {
    return;
  }
  opts->struct_version = DWARFS_C_WRITER_OPTIONS_VERSION;
  opts->compression = DWARFS_C_COMPRESSION_ZSTD;
  opts->compression_level = -1;
  opts->block_size_bits = 0;
  opts->enable_categorizer = 0;
  opts->num_workers = 0;
}

DWARFS_C_API
dwarfs_c_writer* dwarfs_c_writer_create(const dwarfs_c_writer_options* opts) {
  clear_error();

  dwarfs_c_writer_options eff;
  if (!opts) {
    dwarfs_c_writer_options_init(&eff);
  } else {
    eff = *opts;
  }

  if (opts && eff.struct_version != DWARFS_C_WRITER_OPTIONS_VERSION) {
    fail(EINVAL, "unsupported dwarfs_c_writer_options struct_version");
    return nullptr;
  }
  if (eff.compression < DWARFS_C_COMPRESSION_NONE ||
      eff.compression > DWARFS_C_COMPRESSION_BROTLI) {
    fail(EINVAL, "invalid compression algorithm");
    return nullptr;
  }
  if (!compression_level_in_range(eff.compression, eff.compression_level)) {
    fail(EINVAL, "compression level out of range for the chosen algorithm");
    return nullptr;
  }
  if (eff.block_size_bits != 0 &&
      (eff.block_size_bits < 10 || eff.block_size_bits > 30)) {
    fail(EINVAL, "block_size_bits must be 0 (default) or in [10, 30]");
    return nullptr;
  }
  if (eff.enable_categorizer != 0 && eff.enable_categorizer != 1) {
    fail(EINVAL, "enable_categorizer must be 0 or 1");
    return nullptr;
  }
  if (eff.block_size_bits == 0) {
    eff.block_size_bits = kDefaultBlockSizeBits;
  }

  auto w = std::make_unique<dwarfs_c_writer>();
  w->opts = eff;
  return w.release();
}

DWARFS_C_API
int dwarfs_c_writer_add_tree(dwarfs_c_writer* w, const char* host_path,
                             const char* image_prefix) {
  clear_error();
  if (!w || !host_path || !*host_path) {
    return fail(EINVAL, "writer and host_path must not be null");
  }
  if (w->written) {
    return fail(EALREADY, "image already written");
  }
  if (image_prefix && *image_prefix &&
      !(image_prefix[0] == '/' && image_prefix[1] == '\0')) {
    return fail(EINVAL,
                "v1 supports only '/' as image_prefix (the tree lands at "
                "the image root)");
  }
  if (w->kind != dwarfs_c_writer::source_kind::none) {
    return fail(EALREADY,
                "writer already has a source (v1 is single-source; see "
                "dwarfs_c.h)");
  }

  std::error_code ec;
  auto const status = std::filesystem::status(host_path, ec);
  if (ec) {
    return fail(ENOENT, "host_path does not exist");
  }
  if (!std::filesystem::is_directory(status)) {
    return fail(ENOTDIR, "host_path is not a directory");
  }

  w->kind = dwarfs_c_writer::source_kind::tree;
  w->tree_path = host_path;
  return 0;
}

DWARFS_C_API
int dwarfs_c_writer_add_file(dwarfs_c_writer* w, const char* host_path,
                             const char* image_path) {
  clear_error();
  if (!w || !host_path || !*host_path || !image_path || !*image_path) {
    return fail(EINVAL, "writer, host_path and image_path must not be null");
  }
  if (w->written) {
    return fail(EALREADY, "image already written");
  }

  std::filesystem::path const hp(host_path);
  std::filesystem::path const base = hp.filename();
  // v1 places files at the image root by basename (no renames, no
  // directories in image_path)
  if (base.empty() || image_path != base.string()) {
    return fail(EINVAL,
                "v1 requires image_path to equal basename(host_path) "
                "(files land at the image root by basename)");
  }
  if (w->kind == dwarfs_c_writer::source_kind::tree) {
    return fail(EALREADY,
                "writer already has a tree source (v1 is single-source; "
                "see dwarfs_c.h)");
  }

  std::error_code ec;
  auto const status = std::filesystem::status(hp, ec);
  if (ec) {
    return fail(ENOENT, "host_path does not exist");
  }
  if (!std::filesystem::is_regular_file(status)) {
    return fail(EINVAL, "host_path is not a regular file");
  }

  auto const parent = hp.parent_path();
  if (w->kind == dwarfs_c_writer::source_kind::none) {
    w->kind = dwarfs_c_writer::source_kind::files;
    w->files_root = parent.empty() ? std::filesystem::path{"."} : parent;
  } else if (w->files_root != (parent.empty() ? std::filesystem::path{"."} : parent)) {
    return fail(EINVAL,
                "v1 requires all add_file sources to share one directory");
  }

  w->file_names.push_back(base.string());
  return 0;
}

DWARFS_C_API
int dwarfs_c_writer_write(dwarfs_c_writer* w, const char* out_path) {
  clear_error();
  if (!w || !out_path || !*out_path) {
    return fail(EINVAL, "writer and out_path must not be null");
  }
  if (w->written) {
    return fail(EALREADY, "image already written");
  }
  if (w->kind == dwarfs_c_writer::source_kind::none) {
    return fail(EINVAL, "no source added (add_tree or add_file first)");
  }

  // The writer never overwrites (same as plain mkdwarfs without --force)
  {
    std::error_code ec;
    if (std::filesystem::exists(out_path, ec)) {
      return fail(EEXIST, "output file exists");
    }
  }

  try {
    using namespace std::chrono_literals;
    auto& lgr = w->lgr;
    auto& os = w->os;
    size_t const num_workers =
        w->opts.num_workers == 0
            ? std::max(std::thread::hardware_concurrency(), 1U)
            : static_cast<size_t>(w->opts.num_workers);

    dwarfs::writer::writer_progress prog(
        [](dwarfs::writer::writer_progress&, bool) {}, 1000ms);
    dwarfs::thread_pool pool(lgr, os, "compress", num_workers,
                             (std::numeric_limits<size_t>::max)(), 5);
    dwarfs::thread_pool scanner_pool(lgr, os, "scanner", num_workers);

    std::ofstream out(out_path, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
      return fail(EIO, "cannot open output file");
    }

    dwarfs::writer::filesystem_writer_options fsopts;
    dwarfs::writer::filesystem_writer fsw(out, lgr, pool, prog, fsopts);

    dwarfs::block_compressor_parser compressor_parser;
    fsw.add_default_compressor(
        compressor_parser.parse(block_compression_spec(w->opts)));
    fsw.add_section_compressor(
        dwarfs::section_type::METADATA_V2_SCHEMA,
        compressor_parser.parse(kSchemaCompression));
    fsw.add_section_compressor(dwarfs::section_type::METADATA_V2,
                               compressor_parser.parse(kMetadataCompression));

    // Scanner options: the mkdwarfs level-7 profile (similarity ordering,
    // sparse files on, history on; categorizers off unless requested)
    dwarfs::writer::scanner_options sopts;
    sopts.num_segmenter_workers = num_workers;
    sopts.metadata.enable_sparse_files = true;

    dwarfs::writer::fragment_order_options order;
    order.mode = dwarfs::writer::fragment_order_mode::NILSIMSA;
    sopts.inode.fragment_order.set_default(order);

    std::shared_ptr<dwarfs::writer::categorizer_manager> catmgr;
    if (w->opts.enable_categorizer) {
      auto const& cat_root = w->kind == dwarfs_c_writer::source_kind::tree
                                 ? w->tree_path
                                 : w->files_root;
      catmgr =
          std::make_shared<dwarfs::writer::categorizer_manager>(lgr, cat_root);
      dwarfs::writer::categorizer_registry catreg;
      boost::program_options::variables_map vm;
      if (auto cat = catreg.create(lgr, "pcmaudio", vm, nullptr)) {
        catmgr->add(std::move(cat));
      }
      sopts.inode.categorizer_mgr = catmgr;
    }

    dwarfs::writer::segmenter_factory::config sf_config;
    sf_config.block_size_bits = w->opts.block_size_bits;
    sf_config.blockhash_window_size.set_default(kWindowSize);
    sf_config.window_increment_shift.set_default(kWindowStep);
    sf_config.max_active_blocks.set_default(kMaxActiveBlocks);
    sf_config.bloom_filter_size.set_default(kBloomFilterSize);

    dwarfs::writer::segmenter_factory sf(lgr, prog, catmgr, sf_config);
    dwarfs::writer::entry_factory ef;
    dwarfs::writer::scanner s(lgr, scanner_pool, sf, ef, os, sopts);

    if (w->kind == dwarfs_c_writer::source_kind::tree) {
      s.scan(fsw, w->tree_path, prog);
    } else {
      std::vector<std::filesystem::path> list;
      list.reserve(w->file_names.size());
      for (auto const& name : w->file_names) {
        list.emplace_back(name);
      }
      std::span<std::filesystem::path const> list_span{list};
      std::optional<std::span<std::filesystem::path const>> list_opt{
          list_span};
      s.scan(fsw, w->files_root, prog, list_opt);
    }

    out.flush();
    if (!out.good()) {
      return fail(EIO, "failed to write the image");
    }
    out.close();
  } catch (std::exception const& e) {
    return fail_from_exception(e);
  } catch (...) {
    return fail(EIO, "unknown error");
  }

  w->written = true;
  return 0;
}

DWARFS_C_API
void dwarfs_c_writer_free(dwarfs_c_writer* w) { delete w; }

} // extern "C"
