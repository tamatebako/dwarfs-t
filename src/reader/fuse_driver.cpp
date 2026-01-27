#include <dwarfs/reader/fuse_driver.h>

#include <cassert>
#include <cstring>
#include <fstream>
#include <mutex>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <fcntl.h>

#include <fmt/format.h>

#include <dwarfs/config.h>

#ifndef DWARFS_FUSE_LOWLEVEL
#define DWARFS_FUSE_LOWLEVEL 1
#endif

#ifdef DWARFS_USE_FUSE_T
// FUSE-T on macOS uses FUSE2 API with headers at fuse/fuse.h
// Headers are included from fuse/ subdirectory but provide FUSE2 API
#if DWARFS_FUSE_LOWLEVEL
#include <fuse/fuse_lowlevel.h>
#else
#include <fuse/fuse.h>
#endif
#elif FUSE_USE_VERSION >= 30
// FUSE3 or macFUSE with FUSE3 API
#if DWARFS_FUSE_LOWLEVEL
#include <fuse3/fuse_lowlevel.h>
#else
#include <fuse3/fuse.h>
#endif
#else
// FUSE2 compatibility
#include <fuse.h>
#if DWARFS_FUSE_LOWLEVEL
#if __has_include(<fuse/fuse_lowlevel.h>)
#include <fuse/fuse_lowlevel.h>
#else
#include <fuse_lowlevel.h>
#endif
#endif
#endif

#ifdef _WIN32
#include <fuse3/winfsp_fuse.h>
#define st_atime st_atim.tv_sec
#define st_ctime st_ctim.tv_sec
#define st_mtime st_mtim.tv_sec
#define DWARFS_FSP_COMPAT
#endif

#if FUSE_USE_VERSION >= 30 && !defined(_WIN32) && !defined(DWARFS_USE_FUSE_T)
#define DWARFS_FUSE_HAS_LSEEK
#endif

#include <dwarfs/error.h>
#include <dwarfs/file_stat.h>
#include <dwarfs/logger.h>
#include <dwarfs/os_access.h>
#include <dwarfs/performance_monitor.h>
#include <dwarfs/reader/filesystem_v2.h>
#include <dwarfs/reader/iovec_read_buf.h>
#include <dwarfs/util.h>
#include <dwarfs/vfs_stat.h>

namespace dwarfs::reader {

namespace {

#ifdef DWARFS_FSP_COMPAT
using native_stat = struct ::fuse_stat;
using native_statvfs = struct ::fuse_statvfs;
using native_off_t = ::fuse_off_t;
#else
using native_stat = struct ::stat;
using native_statvfs = struct ::statvfs;
using native_off_t = ::off_t;
#endif

constexpr size_t const kMaxInodeInfoChunks{8};

constexpr std::string_view pid_xattr{"user.dwarfs.driver.pid"};
constexpr std::string_view perfmon_xattr{"user.dwarfs.driver.perfmon"};
constexpr std::string_view inodeinfo_xattr{"user.dwarfs.inodeinfo"};

#ifndef O_ACCMODE
#define O_ACCMODE (O_RDONLY | O_WRONLY | O_RDWR)
#endif

/**
 * Helper class for tracking analyzed files
 */
class dwarfs_analysis {
 public:
  explicit dwarfs_analysis(std::filesystem::path const& path)
      : ofs_{path} {
    if (!ofs_) {
      throw std::system_error{errno, std::system_category()};
    }
  }

  void add_lookup(uint32_t ino, std::string const& path) {
    std::lock_guard lock{mx_};
    lookup_.try_emplace(ino, path);
  }

  void add_open(uint32_t ino) {
    std::lock_guard lock{mx_};
    if (opened_.insert(ino).second) {
      ofs_ << lookup_.at(ino) << '\n';
      ofs_.flush();
    }
  }

 private:
  std::mutex mx_;
  std::ofstream ofs_;
  std::unordered_map<uint32_t, std::string> lookup_;
  std::unordered_set<uint32_t> opened_;
};

#if !DWARFS_FUSE_LOWLEVEL
std::optional<inode_view>
find_inode(PERFMON_SECTION_PARAM_ filesystem_v2_lite& fs,
           std::string_view path) {
  auto dev = fs.find(path);
  if (dev) {
    auto iv = dev->inode();
    PERFMON_SET_CONTEXT(iv.inode_num())
    return iv;
  }
  return std::nullopt;
}
#endif

} // anonymous namespace

/**
 * Implementation details for fuse_driver
 */
struct fuse_driver::impl {
  filesystem_v2_lite& fs;
  logger& lgr;
  os_access const& os;
  fuse_driver_config config;
  std::optional<dwarfs_analysis> analysis;
#ifdef DWARFS_FUSE_HAS_LSEEK
  bool fs_has_sparse_files{false};
#endif

  // Performance monitoring
  PERFMON_EXT_PROXY_DECL
  PERFMON_EXT_TIMER_DECL(op_init)
  PERFMON_EXT_TIMER_DECL(op_lookup)
  PERFMON_EXT_TIMER_DECL(op_getattr)
  PERFMON_EXT_TIMER_DECL(op_readlink)
  PERFMON_EXT_TIMER_DECL(op_open)
#ifdef DWARFS_FUSE_HAS_LSEEK
  PERFMON_EXT_TIMER_DECL(op_lseek)
#endif
  PERFMON_EXT_TIMER_DECL(op_read)
  PERFMON_EXT_TIMER_DECL(op_readdir)
  PERFMON_EXT_TIMER_DECL(op_statfs)
  PERFMON_EXT_TIMER_DECL(op_getxattr)
  PERFMON_EXT_TIMER_DECL(op_listxattr)

  impl(filesystem_v2_lite& fs_, fuse_driver_config const& cfg,
       logger& lgr_, os_access const& os_)
      : fs{fs_}
      , lgr{lgr_}
      , os{os_}
      , config{cfg} {
    // Setup analysis if configured
    if (config.analysis_file) {
      analysis.emplace(*config.analysis_file);
    }

    // Setup performance monitoring
#if DWARFS_PERFMON_ENABLED
    if (config.perfmon) {
      PERFMON_EXT_PROXY_SETUP(*this, config.perfmon, "fuse")
      PERFMON_EXT_TIMER_SETUP(*this, op_init)
      PERFMON_EXT_TIMER_SETUP(*this, op_lookup, "inode")
      PERFMON_EXT_TIMER_SETUP(*this, op_getattr, "inode")
      PERFMON_EXT_TIMER_SETUP(*this, op_readlink, "inode")
      PERFMON_EXT_TIMER_SETUP(*this, op_open, "inode")
#ifdef DWARFS_FUSE_HAS_LSEEK
      PERFMON_EXT_TIMER_SETUP(*this, op_lseek, "inode")
#endif
      PERFMON_EXT_TIMER_SETUP(*this, op_read, "inode", "size")
      PERFMON_EXT_TIMER_SETUP(*this, op_readdir, "inode", "size")
      PERFMON_EXT_TIMER_SETUP(*this, op_statfs)
      PERFMON_EXT_TIMER_SETUP(*this, op_getxattr, "inode")
      PERFMON_EXT_TIMER_SETUP(*this, op_listxattr, "inode")
#endif
    }
  }

  // Helper functions
  template <typename LogProxy, typename T>
  auto checked_call(LogProxy& log_, T&& f) -> decltype(std::forward<T>(f)()) {
    try {
      return std::forward<T>(f)();
    } catch (dwarfs::system_error const& e) {
      LOG_ERROR << exception_str(e);
      return e.get_errno();
    } catch (std::system_error const& e) {
      LOG_ERROR << exception_str(e);
      return e.code().value();
    } catch (std::exception const& e) {
      LOG_ERROR << exception_str(e);
      return EIO;
    }
  }

#if DWARFS_FUSE_LOWLEVEL
  template <typename LogProxy, typename T>
  void checked_reply_err(LogProxy& log_, fuse_req_t req, T&& f) {
    int err = checked_call(log_, std::forward<T>(f));
    if (err != 0) {
      fuse_reply_err(req, err);
    }
  }

  static std::string get_caller_context(fuse_req_t req) {
    auto ctx = fuse_req_ctx(req);
    return fmt::format(" [pid={}, uid={}, gid={}]", ctx->pid, ctx->uid,
                       ctx->gid);
  }
#else
  static std::string get_caller_context() {
    auto ctx = fuse_get_context();
    return fmt::format(" [pid={}, uid={}, gid={}]", ctx->pid, ctx->uid,
                       ctx->gid);
  }
#endif

  // Readdir policy classes
#if DWARFS_FUSE_LOWLEVEL
  class readdir_lowlevel_policy {
   public:
    readdir_lowlevel_policy(fuse_req_t req, fuse_ino_t ino, size_t size)
        : req_{req}
        , ino_{ino} {
      buf_.resize(size);
    }

    auto find(filesystem_v2_lite& fs) const { return fs.find(ino_); }

    bool keep_going() const { return written_ < buf_.size(); }

    bool add_entry(std::string const& name, native_stat const& st,
                   file_off_t off) {
      assert(written_ < buf_.size());
      auto needed =
          fuse_add_direntry(req_, &buf_[written_], buf_.size() - written_,
                            name.c_str(), &st, off + 1);
      if (written_ + needed > buf_.size()) {
        return false;
      }
      written_ += needed;
      return true;
    }

    void finalize() const {
      fuse_reply_buf(req_, written_ > 0 ? buf_.data() : nullptr, written_);
    }

   private:
    fuse_req_t req_;
    fuse_ino_t ino_;
    std::vector<char> buf_;
    size_t written_{0};
  };
#else
  class readdir_policy {
   public:
    readdir_policy(char const* path, void* buf, fuse_fill_dir_t filler)
        : path_{path}
        , buf_{buf}
        , filler_{filler} {}

    auto find(filesystem_v2_lite& fs) const {
      std::optional<inode_view> iv;
      if (auto dev = fs.find(path_)) {
        iv = dev->inode();
      }
      return iv;
    }

    bool keep_going() const { return true; }

    bool add_entry(std::string const& name, native_stat const& st,
                   file_off_t off) {
#if defined(DWARFS_USE_FUSE_T) || FUSE_USE_VERSION < 30
      // FUSE2 and FUSE-T use 4-parameter filler (no flags argument)
      // For FUSE-T, offset must be 0
#ifdef DWARFS_USE_FUSE_T
      return filler_(buf_, name.c_str(), &st, 0) == 0;
#else
      return filler_(buf_, name.c_str(), &st, off + 1) == 0;
#endif
#else
      // FUSE3 uses 5-parameter filler with flags
      return filler_(buf_, name.c_str(), &st, off + 1, FUSE_FILL_DIR_PLUS) ==
             0;
#endif
    }

    void finalize() const {}

   private:
    char const* path_;
    void* buf_;
    fuse_fill_dir_t filler_;
  };
#endif

  // FUSE operations implementations
  template <typename LoggerPolicy>
  void op_init_impl(void* data) {
    PERFMON_EXT_SCOPED_SECTION(*this, op_init)
    LOG_PROXY(LoggerPolicy, lgr);

    LOG_DEBUG << __func__;

    // Set num workers after FUSE has forked
    fs.set_num_workers(config.num_workers);

    // Set cache tidy config after FUSE has forked
    fs.set_cache_tidy_config(config.tidy);

    // Preload blocks if configured
    if (config.preload_category) {
      fs.cache_blocks_by_category(*config.preload_category);
    } else if (config.preload_all) {
      fs.cache_all_blocks();
    }

#ifdef DWARFS_FUSE_HAS_LSEEK
    fs_has_sparse_files = fs.has_sparse_files();
#endif
  }

#if DWARFS_FUSE_LOWLEVEL
  template <typename LoggerPolicy>
  static void op_init(void* raw_impl, struct fuse_conn_info* /*conn*/) {
    auto* pimpl = reinterpret_cast<impl*>(raw_impl);
    pimpl->op_init_impl<LoggerPolicy>(raw_impl);
  }

  template <typename LoggerPolicy>
  static void op_lookup(fuse_req_t req, fuse_ino_t parent, char const* name) {
    auto* pimpl = reinterpret_cast<impl*>(fuse_req_userdata(req));
    PERFMON_EXT_SCOPED_SECTION(*pimpl, op_lookup)
    LOG_PROXY(LoggerPolicy, pimpl->lgr);

    LOG_DEBUG << __func__ << "(" << parent << ", " << name << ")"
              << pimpl->get_caller_context(req);

    pimpl->checked_reply_err(log_, req, [&] {
      auto dev = pimpl->fs.find(parent, name);

      if (!dev) {
        return ENOENT;
      }

      if (pimpl->analysis) {
        auto iv = dev->inode();
        if (iv.is_regular_file()) {
          pimpl->analysis->add_lookup(iv.inode_num(), dev->path());
        }
      }

      std::error_code ec;
      auto stbuf = pimpl->fs.getattr(dev->inode(), ec);

      if (!ec) {
        struct ::fuse_entry_param e;

        e.attr = {};
        stbuf.copy_to(&e.attr);
        e.generation = 1;
        e.ino = e.attr.st_ino;
        e.attr_timeout = std::numeric_limits<double>::max();
        e.entry_timeout = std::numeric_limits<double>::max();

        PERFMON_SET_CONTEXT(e.ino)

        fuse_reply_entry(req, &e);
      }

      return ec.value();
    });
  }
#endif

  template <typename LogProxy, typename Find>
  int op_getattr_common(LogProxy& log_, native_stat* st, Find const& find) {
    return checked_call(log_, [&] {
      auto iv = find();

      if (!iv) {
        return ENOENT;
      }

      std::error_code ec;
      auto stbuf = fs.getattr(*iv, ec);

      if (!ec) {
        *st = {};
        stbuf.copy_to(st);
      }

      return ec.value();
    });
  }

#if DWARFS_FUSE_LOWLEVEL
  template <typename LoggerPolicy>
  static void op_getattr(fuse_req_t req, fuse_ino_t ino,
                         struct fuse_file_info*) {
    auto* pimpl = reinterpret_cast<impl*>(fuse_req_userdata(req));
    PERFMON_EXT_SCOPED_SECTION(*pimpl, op_getattr)
    LOG_PROXY(LoggerPolicy, pimpl->lgr);

    LOG_DEBUG << __func__ << "(" << ino << ")"
              << pimpl->get_caller_context(req);
    PERFMON_SET_CONTEXT(ino)

    native_stat st;

    int err = pimpl->op_getattr_common(log_, &st,
                                       [&] { return pimpl->fs.find(ino); });

    if (err == 0) {
      fuse_reply_attr(req, &st, std::numeric_limits<double>::max());
    } else {
      fuse_reply_err(req, err);
    }
  }
#else
  template <typename LoggerPolicy>
  static int op_getattr(char const* path, native_stat* st
#if defined(DWARFS_USE_FUSE_T) || FUSE_USE_VERSION < 30
  ) {
#else
                        , struct fuse_file_info*) {
#endif
    auto* pimpl =
        reinterpret_cast<impl*>(fuse_get_context()->private_data);
    PERFMON_EXT_SCOPED_SECTION(*pimpl, op_getattr)
    LOG_PROXY(LoggerPolicy, pimpl->lgr);

    LOG_DEBUG << __func__ << "(" << path << ")"
              << pimpl->get_caller_context();

    return -pimpl->op_getattr_common(log_, st, [&] {
      return find_inode(PERFMON_SECTION_ARG_ pimpl->fs, path);
    });
  }
#endif

  template <typename LogProxy, typename Find>
  int op_readlink_common(LogProxy& log_, std::string* str,
                         Find const& find) {
    return checked_call(log_, [&] {
      if (auto iv = find()) {
        std::error_code ec;
        auto link = fs.readlink(*iv, readlink_mode::posix, ec);
        if (!ec) {
          *str = link;
        }
        return ec.value();
      }
      return ENOENT;
    });
  }

#if DWARFS_FUSE_LOWLEVEL
  template <typename LoggerPolicy>
  static void op_readlink(fuse_req_t req, fuse_ino_t ino) {
    auto* pimpl = reinterpret_cast<impl*>(fuse_req_userdata(req));
    PERFMON_EXT_SCOPED_SECTION(*pimpl, op_readlink)
    LOG_PROXY(LoggerPolicy, pimpl->lgr);

    LOG_DEBUG << __func__ << "(" << ino << ")"
              << pimpl->get_caller_context(req);
    PERFMON_SET_CONTEXT(ino)

    std::string symlink;

    auto err = pimpl->op_readlink_common(log_, &symlink,
                                         [&] { return pimpl->fs.find(ino); });

    if (err == 0) {
      fuse_reply_readlink(req, symlink.c_str());
    } else {
      fuse_reply_err(req, err);
    }
  }
#else
  template <typename LoggerPolicy>
  static int op_readlink(char const* path, char* buf, size_t buflen) {
    auto* pimpl =
        reinterpret_cast<impl*>(fuse_get_context()->private_data);
    PERFMON_EXT_SCOPED_SECTION(*pimpl, op_readlink)
    LOG_PROXY(LoggerPolicy, pimpl->lgr);

    LOG_DEBUG << __func__ << "(" << path << ")"
              << pimpl->get_caller_context();

    std::string symlink;

    auto err = pimpl->op_readlink_common(log_, &symlink, [&] {
      return find_inode(PERFMON_SECTION_ARG_ pimpl->fs, path);
    });

    if (err == 0) {
#ifdef _WIN32
      ::strncpy_s(buf, buflen, symlink.data(), symlink.size());
#else
      ::strncpy(buf, symlink.data(), buflen);
#endif
    }

    return -err;
  }
#endif

  template <typename LogProxy, typename Find>
  int op_open_common(LogProxy& log_, struct fuse_file_info* fi,
                     Find const& find) {
    return checked_call(log_, [&] {
      auto iv = find();

      if (!iv) {
        return ENOENT;
      }

      if (iv->is_directory()) {
        return EISDIR;
      }

      if ((fi->flags & O_ACCMODE) != O_RDONLY ||
          (fi->flags & (O_APPEND | O_TRUNC)) != 0) {
        return EACCES;
      }

      if (analysis) {
        analysis->add_open(iv->inode_num());
      }

      bool do_cache = config.cache_files;

#ifdef DWARFS_FUSE_HAS_LSEEK
      if (do_cache && !config.cache_sparse && fs_has_sparse_files) {
        std::error_code ec;
        auto stat = fs.getattr(*iv, ec);

        if (!ec) {
          if (stat.size() != stat.allocated_size()) {
            do_cache = false;
            LOG_DEBUG << "disabling cache for sparse inode "
                      << iv->inode_num();
          }
        } else {
          LOG_DEBUG << "getattr failed unexpectedly for inode "
                    << iv->inode_num() << ": " << ec.message();
        }
      }
#endif

      fi->fh = iv->inode_num();
      fi->direct_io = !do_cache;
      fi->keep_cache = do_cache;

      return 0;
    });
  }

#if DWARFS_FUSE_LOWLEVEL
  template <typename LoggerPolicy>
  static void op_open(fuse_req_t req, fuse_ino_t ino,
                      struct fuse_file_info* fi) {
    auto* pimpl = reinterpret_cast<impl*>(fuse_req_userdata(req));
    PERFMON_EXT_SCOPED_SECTION(*pimpl, op_open)
    LOG_PROXY(LoggerPolicy, pimpl->lgr);

    LOG_DEBUG << __func__ << "(" << ino << ")"
              << pimpl->get_caller_context(req);
    PERFMON_SET_CONTEXT(ino)

    auto err = pimpl->op_open_common(log_, fi,
                                     [&] { return pimpl->fs.find(ino); });

    if (err == 0) {
      fuse_reply_open(req, fi);
    } else {
      fuse_reply_err(req, err);
    }
  }
#else
  template <typename LoggerPolicy>
  static int op_open(char const* path, struct fuse_file_info* fi) {
    auto* pimpl =
        reinterpret_cast<impl*>(fuse_get_context()->private_data);
    PERFMON_EXT_SCOPED_SECTION(*pimpl, op_open)
    LOG_PROXY(LoggerPolicy, pimpl->lgr);

    LOG_DEBUG << __func__ << "(" << path << ")"
              << pimpl->get_caller_context();

    if (pimpl->analysis) {
      auto dev = pimpl->fs.find(path);
      if (dev) {
        auto iv = dev->inode();
        if (iv.is_regular_file()) {
          pimpl->analysis->add_lookup(iv.inode_num(), dev->path());
        }
      }
    }

    return -pimpl->op_open_common(log_, fi, [&] {
      return find_inode(PERFMON_SECTION_ARG_ pimpl->fs, path);
    });
  }
#endif

#ifdef DWARFS_FUSE_HAS_LSEEK
  template <typename LogProxy>
  off_t op_lseek_common(LogProxy& log_, uint32_t inode, off_t off,
                        int whence) {
    return checked_call(log_, [&]() -> off_t {
      seek_whence rwhence;

      switch (whence) {
      case SEEK_DATA:
        rwhence = seek_whence::data;
        break;
      case SEEK_HOLE:
        rwhence = seek_whence::hole;
        break;
      default:
        return -EINVAL;
      }

      std::error_code ec;
      auto offset = fs.seek(inode, off, rwhence, ec);

      if (ec) {
        return -ec.value();
      }

      return offset;
    });
  }

#if DWARFS_FUSE_LOWLEVEL
  template <typename LoggerPolicy>
  static void op_lseek(fuse_req_t req, fuse_ino_t ino, off_t off, int whence,
                       struct fuse_file_info* fi) {
    auto* pimpl = reinterpret_cast<impl*>(fuse_req_userdata(req));
    PERFMON_EXT_SCOPED_SECTION(*pimpl, op_lseek)
    LOG_PROXY(LoggerPolicy, pimpl->lgr);

    LOG_DEBUG << __func__ << "(" << ino << ", " << off << ", " << whence
              << ")" << pimpl->get_caller_context(req);
    PERFMON_SET_CONTEXT(ino)

    if (FUSE_ROOT_ID + fi->fh != ino) {
      fuse_reply_err(req, EIO);
    }

    auto result = pimpl->op_lseek_common(log_, ino, off, whence);

    if (result >= 0) {
      fuse_reply_lseek(req, result);
    } else {
      fuse_reply_err(req, -static_cast<int>(result));
    }
  }
#else
  template <typename LoggerPolicy>
  static off_t op_lseek(char const* path, off_t off, int whence,
                        struct fuse_file_info* fi) {
    auto* pimpl =
        reinterpret_cast<impl*>(fuse_get_context()->private_data);
    PERFMON_EXT_SCOPED_SECTION(*pimpl, op_lseek)
    LOG_PROXY(LoggerPolicy, pimpl->lgr);
    PERFMON_SET_CONTEXT(fi->fh)

    LOG_DEBUG << __func__ << "(" << path << ", " << off << ", " << whence
              << ")" << pimpl->get_caller_context();

    return pimpl->op_lseek_common(log_, fi->fh, off, whence);
  }
#endif
#endif

#if DWARFS_FUSE_LOWLEVEL
  template <typename LoggerPolicy>
  static void op_read(fuse_req_t req, fuse_ino_t ino, size_t size,
                      file_off_t off, struct fuse_file_info* fi) {
    auto* pimpl = reinterpret_cast<impl*>(fuse_req_userdata(req));
    PERFMON_EXT_SCOPED_SECTION(*pimpl, op_read)
    LOG_PROXY(LoggerPolicy, pimpl->lgr);

    LOG_DEBUG << __func__ << "(" << ino << ", " << size << ", " << off << ")"
              << pimpl->get_caller_context(req);
    PERFMON_SET_CONTEXT(ino, size)

    pimpl->checked_reply_err(log_, req, [&]() -> ssize_t {
      if (FUSE_ROOT_ID + fi->fh != ino) {
        return EIO;
      }

      std::error_code ec;
      iovec_read_buf buf;
      auto num = pimpl->fs.readv(ino, buf, size, off, ec);

      LOG_DEBUG << "readv(" << ino << ", " << size << ", " << off << ") -> "
                << num << " [size = " << buf.buf.size()
                << "]: " << ec.message();

      if (ec) {
        return ec.value();
      }

      return -fuse_reply_iov(req, buf.buf.empty() ? nullptr : buf.buf.data(),
                             buf.buf.size());
    });
  }
#else
  template <typename LoggerPolicy>
  static int op_read(char const* path, char* buf, size_t size,
                     native_off_t off, struct fuse_file_info* fi) {
    auto* pimpl =
        reinterpret_cast<impl*>(fuse_get_context()->private_data);
    PERFMON_EXT_SCOPED_SECTION(*pimpl, op_read)
    LOG_PROXY(LoggerPolicy, pimpl->lgr);

    LOG_DEBUG << __func__ << "(" << path << ", " << size << ", " << off << ")"
              << pimpl->get_caller_context();
    PERFMON_SET_CONTEXT(fi->fh, size)

    return -pimpl->checked_call(log_, [&] {
      auto rv = pimpl->fs.read(fi->fh, buf, size, off);

      LOG_DEBUG << "read(" << path << " [" << fi->fh << "], " << size << ", "
                << off << ") -> " << rv;

      return -rv;
    });
  }
#endif

  template <typename Policy, typename OnInode>
  int op_readdir_common(Policy& policy, file_off_t off, OnInode&& on_inode) {
    auto iv = policy.find(fs);

    if (!iv) {
      return ENOENT;
    }

    std::forward<OnInode>(on_inode)(*iv);

    auto dir = fs.opendir(*iv);

    if (!dir) {
      return ENOTDIR;
    }

    file_off_t lastoff = fs.dirsize(*dir);
    native_stat st;

    st = {};

    while (off < lastoff && policy.keep_going()) {
      auto dev = fs.readdir(*dir, off);
      assert(dev);

      std::error_code ec;
      auto stbuf = fs.getattr(dev->inode(), ec);

      if (ec) {
        return ec.value();
      }

      stbuf.copy_to(&st);

      if (!policy.add_entry(dev->name(), st, off)) {
        break;
      }

      ++off;
    }

    policy.finalize();

    return 0;
  }

#if DWARFS_FUSE_LOWLEVEL
  template <typename LoggerPolicy>
  static void op_readdir(fuse_req_t req, fuse_ino_t ino, size_t size,
                         file_off_t off, struct fuse_file_info* /*fi*/) {
    auto* pimpl = reinterpret_cast<impl*>(fuse_req_userdata(req));
    PERFMON_EXT_SCOPED_SECTION(*pimpl, op_readdir)
    LOG_PROXY(LoggerPolicy, pimpl->lgr);

    LOG_DEBUG << __func__ << "(" << ino << ", " << size << ", " << off << ")"
              << pimpl->get_caller_context(req);
    PERFMON_SET_CONTEXT(ino, size)

    pimpl->checked_reply_err(log_, req, [&] {
      typename impl::readdir_lowlevel_policy policy{req, ino, size};
      return pimpl->op_readdir_common(policy, off, [](auto const&) {});
    });
  }
#else
#if defined(DWARFS_USE_FUSE_T) || FUSE_USE_VERSION < 30
  // FUSE2 and FUSE-T use 5-parameter readdir (no flags argument)
  template <typename LoggerPolicy>
  static int op_readdir(char const* path, void* buf, fuse_fill_dir_t filler,
                        native_off_t off, struct fuse_file_info* /*fi*/) {
#else
  // FUSE3 uses 6-parameter readdir with flags
  template <typename LoggerPolicy>
  static int op_readdir(char const* path, void* buf, fuse_fill_dir_t filler,
                        native_off_t off, struct fuse_file_info* /*fi*/,
                        enum fuse_readdir_flags /*flags*/) {
#endif
    auto* pimpl =
        reinterpret_cast<impl*>(fuse_get_context()->private_data);
    PERFMON_EXT_SCOPED_SECTION(*pimpl, op_readdir)
    LOG_PROXY(LoggerPolicy, pimpl->lgr);

    LOG_DEBUG << __func__ << "(" << path << ", " << off << ")"
              << pimpl->get_caller_context();

    return -pimpl->checked_call(log_, [&] {
      typename impl::readdir_policy policy{path, buf, filler};
      return pimpl->op_readdir_common(policy, off, [&](auto e) {
        PERFMON_SET_CONTEXT(e.inode_num())
      });
    });
  }
#endif

  template <typename LogProxy>
  int op_statfs_common(LogProxy& log_, native_statvfs* st) {
    return checked_call(log_, [&] {
      vfs_stat stbuf;

      fs.statvfs(&stbuf);

      *st = {};
      copy_vfs_stat(st, stbuf);

#ifndef _WIN32
      if (stbuf.readonly) {
        st->f_flag |= ST_RDONLY;
      }
#endif

      return 0;
    });
  }

#if DWARFS_FUSE_LOWLEVEL
  template <typename LoggerPolicy>
  static void op_statfs(fuse_req_t req, fuse_ino_t ino) {
    auto* pimpl = reinterpret_cast<impl*>(fuse_req_userdata(req));
    PERFMON_EXT_SCOPED_SECTION(*pimpl, op_statfs)
    LOG_PROXY(LoggerPolicy, pimpl->lgr);

    LOG_DEBUG << __func__ << "(" << ino << ")"
              << pimpl->get_caller_context(req);

    struct ::statvfs st;

    int err = pimpl->op_statfs_common(log_, &st);

    if (err == 0) {
      fuse_reply_statfs(req, &st);
    } else {
      fuse_reply_err(req, err);
    }
  }
#else
  template <typename LoggerPolicy>
  static int op_statfs(char const* path, native_statvfs* st) {
    auto* pimpl =
        reinterpret_cast<impl*>(fuse_get_context()->private_data);
    PERFMON_EXT_SCOPED_SECTION(*pimpl, op_statfs)
    LOG_PROXY(LoggerPolicy, pimpl->lgr);

    LOG_DEBUG << __func__ << "(" << path << ")"
              << pimpl->get_caller_context();

    return -pimpl->op_statfs_common(log_, st);
  }
#endif

  template <typename LogProxy, typename Find>
  int op_getxattr_common(LogProxy& log_, std::string_view name,
                         std::string& value,
                         size_t& extra_size [[maybe_unused]],
                         Find const& find) {
    return checked_call(log_, [&] {
      auto iv = find();

      if (!iv) {
        return ENOENT;
      }

      std::ostringstream oss;

      if (iv->inode_num() == 0) {
        if (name == pid_xattr) {
          oss << std::to_string(::getpid());
        } else if (name == perfmon_xattr) {
#if DWARFS_PERFMON_ENABLED
          if (config.perfmon) {
            config.perfmon->summarize(oss);
            extra_size = 4096;
          } else {
            oss << "performance monitor is disabled\n";
          }
#else
          oss << "no performance monitor support\n";
#endif
        }
      }

      if (name == inodeinfo_xattr) {
        oss << fs.get_inode_info(*iv, kMaxInodeInfoChunks) << "\n";
      }

      value = oss.str();

      if (value.empty()) {
#ifdef __APPLE__
        return ENOATTR;
#else
        return ENODATA;
#endif
      }

      return 0;
    });
  }

#if DWARFS_FUSE_LOWLEVEL
  template <typename LoggerPolicy>
  static void op_getxattr(fuse_req_t req, fuse_ino_t ino, char const* name,
                          size_t size
#ifdef __APPLE__
                          ,
                          uint32_t position
#endif
  ) {
    static constexpr std::string_view fname{__func__};
    auto* pimpl = reinterpret_cast<impl*>(fuse_req_userdata(req));
    PERFMON_EXT_SCOPED_SECTION(*pimpl, op_getxattr)
    LOG_PROXY(LoggerPolicy, pimpl->lgr);

    LOG_DEBUG << fname << "(" << ino << ", " << name << ", " << size
#ifdef __APPLE__
              << ", " << position
#endif
              << ")" << pimpl->get_caller_context(req);
    PERFMON_SET_CONTEXT(ino)

    pimpl->checked_reply_err(log_, req, [&] {
      std::string value;
      size_t extra_size{0};
      auto err = pimpl->op_getxattr_common(log_, name, value, extra_size,
                                           [&] { return pimpl->fs.find(ino); });

      if (err != 0) {
        LOG_TRACE << fname << ": err=" << err;
        return err;
      }

      LOG_TRACE << fname << ": value.size=" << value.size()
                << ", extra_size=" << extra_size;

      if (size == 0) {
        fuse_reply_xattr(req, value.size() + extra_size);
        return 0;
      }

      if (size >= value.size()) {
        fuse_reply_buf(req, value.data(), value.size());
        return 0;
      }

      return ERANGE;
    });
  }
#else
  template <typename LoggerPolicy>
  static int op_getxattr(char const* path, char const* name, char* value,
                         size_t size
#if defined(DWARFS_USE_FUSE_T)
                         , uint32_t /*position*/
#endif
                         ) {
    auto* pimpl =
        reinterpret_cast<impl*>(fuse_get_context()->private_data);
    PERFMON_EXT_SCOPED_SECTION(*pimpl, op_getxattr)
    LOG_PROXY(LoggerPolicy, pimpl->lgr);

    LOG_DEBUG << __func__ << "(" << path << ", " << name << ", " << size
              << ")" << pimpl->get_caller_context();

    std::string tmp;
    size_t extra_size{0};
    auto err = pimpl->op_getxattr_common(log_, name, tmp, extra_size, [&] {
      return find_inode(PERFMON_SECTION_ARG_ pimpl->fs, path);
    });

    if (err != 0) {
      LOG_TRACE << __func__ << ": err=" << err;
      return -err;
    }

    LOG_TRACE << __func__ << ": value.size=" << tmp.size()
              << ", extra_size=" << extra_size;

    if (value) {
      if (size < tmp.size()) {
        return -ERANGE;
      }

      std::memcpy(value, tmp.data(), tmp.size());

      return tmp.size();
    }

    return tmp.size() + extra_size;
  }

  template <typename LoggerPolicy>
  static int op_setxattr(char const* path, char const* name,
                         char const* /*value*/, size_t size, int /*flags*/
#if defined(DWARFS_USE_FUSE_T)
                         , uint32_t /*position*/
#endif
                         ) {
    auto* pimpl =
        reinterpret_cast<impl*>(fuse_get_context()->private_data);
    LOG_PROXY(LoggerPolicy, pimpl->lgr);

    LOG_DEBUG << __func__ << "(" << path << ", " << name << ", " << size
              << ")" << pimpl->get_caller_context();

    return -ENOTSUP;
  }

  template <typename LoggerPolicy>
  static int op_removexattr(char const* path, char const* name) {
    auto* pimpl =
        reinterpret_cast<impl*>(fuse_get_context()->private_data);
    LOG_PROXY(LoggerPolicy, pimpl->lgr);

    LOG_DEBUG << __func__ << "(" << path << ", " << name << ")"
              << pimpl->get_caller_context();

    return -ENOTSUP;
  }
#endif

  template <typename LogProxy, typename Find>
  int op_listxattr_common(LogProxy& log_, std::string& xattr_names,
                          Find const& find) {
    return checked_call(log_, [&] {
      auto iv = find();

      if (!iv) {
        return ENOENT;
      }

      std::ostringstream oss;

      if (iv->inode_num() == 0) {
        oss << pid_xattr << '\0';
        oss << perfmon_xattr << '\0';
      }

      oss << inodeinfo_xattr << '\0';

      xattr_names = oss.str();

      return 0;
    });
  }

#if DWARFS_FUSE_LOWLEVEL
  template <typename LoggerPolicy>
  static void op_listxattr(fuse_req_t req, fuse_ino_t ino, size_t size) {
    static constexpr std::string_view fname{__func__};
    auto* pimpl = reinterpret_cast<impl*>(fuse_req_userdata(req));
    PERFMON_EXT_SCOPED_SECTION(*pimpl, op_listxattr)
    LOG_PROXY(LoggerPolicy, pimpl->lgr);

    LOG_DEBUG << fname << "(" << ino << ", " << size << ")"
              << pimpl->get_caller_context(req);
    PERFMON_SET_CONTEXT(ino)

    pimpl->checked_reply_err(log_, req, [&] {
      std::string xattrs;
      auto err = pimpl->op_listxattr_common(log_, xattrs,
                                            [&] { return pimpl->fs.find(ino); });

      if (err != 0) {
        return err;
      }

      LOG_TRACE << fname << ": xattrs.size=" << xattrs.size();

      if (size == 0) {
        fuse_reply_xattr(req, xattrs.size());
        return 0;
      }

      if (size >= xattrs.size()) {
        fuse_reply_buf(req, xattrs.data(), xattrs.size());
        return 0;
      }

      return ERANGE;
    });
  }
#else
  template <typename LoggerPolicy>
  static int op_listxattr(char const* path, char* list, size_t size) {
    auto* pimpl =
        reinterpret_cast<impl*>(fuse_get_context()->private_data);
    PERFMON_EXT_SCOPED_SECTION(*pimpl, op_listxattr)
    LOG_PROXY(LoggerPolicy, pimpl->lgr);

    LOG_DEBUG << __func__ << "(" << path << ", " << size << ")"
              << pimpl->get_caller_context();

    std::string xattrs;
    auto err = pimpl->op_listxattr_common(log_, xattrs, [&] {
      return find_inode(PERFMON_SECTION_ARG_ pimpl->fs, path);
    });

    if (err != 0) {
      return -err;
    }

    if (list) {
      if (size < xattrs.size()) {
        return -ERANGE;
      }

      std::memcpy(list, xattrs.data(), xattrs.size());
    }

    return xattrs.size();
  }
#endif

#if !DWARFS_FUSE_LOWLEVEL
  template <typename LoggerPolicy>
  static int op_rename(char const* from, char const* to
#if !defined(DWARFS_USE_FUSE_T) && FUSE_USE_VERSION >= 30
                       , unsigned int flags
#endif
                       ) {
    auto* pimpl =
        reinterpret_cast<impl*>(fuse_get_context()->private_data);
    LOG_PROXY(LoggerPolicy, pimpl->lgr);

#if defined(DWARFS_USE_FUSE_T)
    LOG_DEBUG << __func__ << "(" << from << ", " << to << ")"
              << pimpl->get_caller_context();
#else
    LOG_DEBUG << __func__ << "(" << from << ", " << to << ", " << flags
              << ")" << pimpl->get_caller_context();
#endif

    return -ENOSYS;
  }
#endif

  // Setup operations wrapper
  template <typename LoggerPolicy>
  void init_fuse_ops_impl(
#if DWARFS_FUSE_LOWLEVEL
      fuse_lowlevel_ops& ops
#else
      fuse_operations& ops
#endif
  ) {
#if DWARFS_FUSE_LOWLEVEL
    ops.init = &impl::op_init<LoggerPolicy>;
    ops.lookup = &impl::op_lookup<LoggerPolicy>;
#else
    // For high-level API, ops.init is optional and can be NULL
#endif
    ops.getattr = &impl::op_getattr<LoggerPolicy>;
    if (fs.has_symlinks()) {
      ops.readlink = &impl::op_readlink<LoggerPolicy>;
    }
    ops.open = &impl::op_open<LoggerPolicy>;
#ifdef DWARFS_FUSE_HAS_LSEEK
    if (fs.has_sparse_files()) {
      ops.lseek = &impl::op_lseek<LoggerPolicy>;
    }
#endif
    ops.read = &impl::op_read<LoggerPolicy>;
    ops.readdir = &impl::op_readdir<LoggerPolicy>;
    ops.statfs = &impl::op_statfs<LoggerPolicy>;
    ops.getxattr = &impl::op_getxattr<LoggerPolicy>;
    ops.listxattr = &impl::op_listxattr<LoggerPolicy>;
#if !DWARFS_FUSE_LOWLEVEL
    ops.setxattr = &impl::op_setxattr<LoggerPolicy>;
    ops.removexattr = &impl::op_removexattr<LoggerPolicy>;
    ops.rename = &impl::op_rename<LoggerPolicy>;
#endif
  }
};

// fuse_driver public interface implementation

fuse_driver::fuse_driver(filesystem_v2_lite& fs,
                         fuse_driver_config const& config, logger& lgr,
                         os_access const& os)
    : impl_{std::make_unique<impl>(fs, config, lgr, os)} {}

fuse_driver::~fuse_driver() = default;

#if DWARFS_FUSE_LOWLEVEL
void fuse_driver::setup_operations(fuse_lowlevel_ops& ops) {
  if (impl_->config.log_threshold >= logger::DEBUG) {
    impl_->init_fuse_ops_impl<debug_logger_policy>(ops);
  } else {
    impl_->init_fuse_ops_impl<prod_logger_policy>(ops);
  }
}
#else
void fuse_driver::setup_operations(fuse_operations& ops) {
  if (impl_->config.log_threshold >= logger::DEBUG) {
    impl_->init_fuse_ops_impl<debug_logger_policy>(ops);
  } else {
    impl_->init_fuse_ops_impl<prod_logger_policy>(ops);
  }
}
#endif

void* fuse_driver::get_userdata() { return impl_.get(); }

} // namespace dwarfs::reader