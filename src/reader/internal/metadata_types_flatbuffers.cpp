// FlatBuffers-based implementation of metadata types
// This file provides the FlatBuffers backend implementation

// Include generated FlatBuffers headers FIRST to avoid namespace collision
#include <dwarfs/gen-flatbuffers/metadata.h>

#include <dwarfs/reader/internal/metadata_types_flatbuffers.h>
#include <dwarfs/reader/internal/time_resolution_handler.h>
#include <dwarfs/metadata/domain/string_table.h>

#include <algorithm>
#include <cassert>
#include <filesystem>
#include <iostream>
#include <ranges>
#include <span>
#include <vector>
#include <memory>
#include <type_traits>
#include <functional>

#include <fmt/format.h>

#include <dwarfs/error.h>
#include <dwarfs/file_stat.h>
#include <dwarfs/logger.h>
#include <dwarfs/util.h>

namespace dwarfs::reader::internal::flatbuffers_backend {

namespace fs = std::filesystem;

//==============================================================================
// global_metadata implementation
//==============================================================================

global_metadata::global_metadata(logger& lgr, Meta const& meta)
    : meta_{meta}
    , names_{[&]() -> dwarfs::internal::string_table {
        // Initialize string table from FlatBuffers names or compact_names
        if (auto compact_names = meta->compact_names()) {
          // Names are compressed - construct domain model string_table
          ::dwarfs::metadata::domain::string_table st;
          if (compact_names->buffer()) {
            st.buffer = compact_names->buffer()->str();
          }
          if (compact_names->symtab() && compact_names->symtab()->size() > 0) {
            // CRITICAL: symtab is binary FSST data, preserve all bytes
            st.symtab = std::string(compact_names->symtab()->data(),
                                    compact_names->symtab()->size());
          }
          if (compact_names->index()) {
            st.index.assign(compact_names->index()->begin(),
                           compact_names->index()->end());
          }
          st.packed_index = compact_names->packed_index();
          return dwarfs::internal::string_table(lgr, "names", st);
        } else if (auto names = meta->names()) {
          // Names are plain - convert to vector
          std::vector<std::string> name_vec;
          name_vec.reserve(names->size());
          for (size_t i = 0; i < names->size(); ++i) {
            if (auto name = names->Get(i)) {
              name_vec.emplace_back(name->str());
            }
          }
          return dwarfs::internal::string_table(std::span<std::string const>(name_vec));
        }
        return dwarfs::internal::string_table(std::vector<std::string>{});
      }()} {
#ifdef DWARFS_DEBUG_STRING_TABLE
  std::cerr << "\n=== DEBUG: String Table Initialized ===" << std::endl;
  std::cerr << "  Entry count: " << names_.size() << std::endl;
  std::cerr << "  Total bytes: " << names_.unpacked_size() << std::endl;

  if (names_.size() == 0) {
    std::cerr << "  ⚠️  WARNING: String table is EMPTY!" << std::endl;
  } else {
    std::cerr << "  First 10 entries:" << std::endl;
    for (size_t i = 0; i < std::min<size_t>(10, names_.size()); ++i) {
      auto name_str = names_[i];
      std::cerr << "    [" << i << "] = '"
                << (name_str.empty() ? "(empty)" : name_str)
                << "'" << std::endl;
    }
  }
  std::cerr << "======================================\n" << std::endl;
#endif
}

void global_metadata::check_consistency(logger& lgr, Meta const& meta) {
  (void)lgr;

  // Basic consistency checks
  if (!meta) {
    DWARFS_THROW(runtime_error, "null metadata pointer");
  }

  auto dirs = meta->directories();
  auto inodes = meta->inodes();
  auto names = meta->names();

  if (!dirs || dirs->size() == 0) {
    DWARFS_THROW(runtime_error, "no directories in metadata");
  }

  if (!inodes || inodes->size() == 0) {
    DWARFS_THROW(runtime_error, "no inodes in metadata");
  }

  // Names can be in either plain 'names' or compressed 'compact_names'
  auto compact_names = meta->compact_names();
  if ((!names || names->size() == 0) && !compact_names) {
    DWARFS_THROW(runtime_error, "no names in metadata");
  }
}

void global_metadata::check_consistency(logger& lgr) const {
  check_consistency(lgr, meta_);
}

uint32_t global_metadata::first_dir_entry(uint32_t ino) const {
  auto dirs = meta_->directories();
  if (dirs && ino < dirs->size()) {
    auto dir = dirs->Get(ino);
    if (dir) {
      return dir->first_entry();
    }
  }
  return 0;
}

uint32_t global_metadata::parent_dir_entry(uint32_t ino) const {
  auto dirs = meta_->directories();
  if (dirs && ino < dirs->size()) {
    auto dir = dirs->Get(ino);
    if (dir) {
      return dir->parent_entry();
    }
  }
  return 0;
}

uint32_t global_metadata::self_dir_entry(uint32_t ino) const {
  auto dirs = meta_->directories();
  if (dirs && ino < dirs->size()) {
    auto dir = dirs->Get(ino);
    if (dir) {
      return ino;  // Default: inode is its own self_entry
    }
  }
  return ino;
}

::dwarfs::flatbuffers::Directory const* global_metadata::get_directory(uint32_t index) const {
  auto dirs = meta_->directories();
  if (dirs && index < dirs->size()) {
    return dirs->Get(index);
  }
  return nullptr;
}

//==============================================================================
// global_metadata interface implementations
//==============================================================================

std::span<uint8_t const> global_metadata::uids() const {
  if (auto uids_vec = meta_->uids()) {
    return std::span<uint8_t const>(
        reinterpret_cast<uint8_t const*>(uids_vec->data()),
        uids_vec->size() * sizeof(uint32_t));
  }
  return {};
}

std::span<uint8_t const> global_metadata::gids() const {
  if (auto gids_vec = meta_->gids()) {
    return std::span<uint8_t const>(
        reinterpret_cast<uint8_t const*>(gids_vec->data()),
        gids_vec->size() * sizeof(uint32_t));
  }
  return {};
}

std::span<uint8_t const> global_metadata::modes() const {
  if (auto modes_vec = meta_->modes()) {
    return std::span<uint8_t const>(
        reinterpret_cast<uint8_t const*>(modes_vec->data()),
        modes_vec->size() * sizeof(uint16_t));
  }
  return {};
}

std::string global_metadata::name_at(uint32_t index) const {
  return names_[index];
}

std::string global_metadata::symlink_at(uint32_t index) const {
  if (auto symlinks = meta_->symlinks()) {
    if (index < symlinks->size()) {
      return symlinks->Get(index)->str();
    }
  }
  return "";
}

uint32_t global_metadata::block_size() const {
  return meta_->block_size();
}

uint64_t global_metadata::total_fs_size() const {
  return meta_->total_fs_size();
}

std::optional<uint32_t> global_metadata::hole_block_index() const {
  auto idx = meta_->hole_block_index();
  return idx != 0 ? std::optional<uint32_t>(idx) : std::nullopt;
}

std::shared_ptr<dir_entry_view_interface const>
global_metadata::make_dir_entry_view(uint32_t index, uint32_t parent_index) const {
  return dir_entry_view_impl::from_dir_entry_index_shared(index, parent_index, *this);
}

std::shared_ptr<dir_entry_view_interface const>
global_metadata::make_dir_entry_view(uint32_t index) const {
  return dir_entry_view_impl::from_dir_entry_index_shared(index, *this);
}

//==============================================================================
// inode_view_impl implementation
//==============================================================================

inode_view_impl::mode_type inode_view_impl::mode() const {
  if (!inode_data_ || !meta_) return 0;
  auto modes = meta_->modes();
  return modes ? modes->Get(inode_data_->mode_index()) : 0;
}

std::string inode_view_impl::mode_string() const {
  return file_stat::mode_string(mode());
}

std::string inode_view_impl::perm_string() const {
  return file_stat::perm_string(mode());
}

inode_view_impl::uid_type inode_view_impl::getuid() const {
  if (!inode_data_ || !meta_) return 0;
  auto uids = meta_->uids();
  return uids ? uids->Get(inode_data_->owner_index()) : 0;
}

inode_view_impl::gid_type inode_view_impl::getgid() const {
  if (!inode_data_ || !meta_) return 0;
  auto gids = meta_->gids();
  return gids ? gids->Get(inode_data_->group_index()) : 0;
}

uint32_t inode_view_impl::nlink_minus_one() const {
  if (!inode_data_) return 0;
  return inode_data_->nlink_minus_one();
}

uint32_t inode_view_impl::inode_v2_2() const {
  if (!inode_data_) return inode_num_;
  auto v22 = inode_data_->inode_v2_2();
  return v22 != 0 ? v22 : inode_num_;
}

uint64_t inode_view_impl::mtime_offset() const {
  return inode_data_ ? inode_data_->mtime_offset() : 0;
}

uint64_t inode_view_impl::atime_offset() const {
  return inode_data_ ? inode_data_->atime_offset() : 0;
}

uint64_t inode_view_impl::ctime_offset() const {
  return inode_data_ ? inode_data_->ctime_offset() : 0;
}

uint64_t inode_view_impl::btime_offset() const {
  return inode_data_ ? inode_data_->btime_offset() : 0;
}

uint64_t inode_view_impl::mtime_subsec() const {
  return inode_data_ ? inode_data_->mtime_subsec() : 0;
}

uint64_t inode_view_impl::atime_subsec() const {
  return inode_data_ ? inode_data_->atime_subsec() : 0;
}

uint64_t inode_view_impl::ctime_subsec() const {
  return inode_data_ ? inode_data_->ctime_subsec() : 0;
}

uint64_t inode_view_impl::btime_subsec() const {
  return inode_data_ ? inode_data_->btime_subsec() : 0;
}

//==============================================================================
// dir_entry_view_impl implementation
//==============================================================================

namespace {

template <typename T>
class shared_ptr_ctor {
 public:
  template <typename... Args>
  auto operator()(Args&&... args) {
    return std::make_shared<T>(std::forward<Args>(args)...);
  }
};

template <typename T>
class stack_ctor {
 public:
  template <typename... Args>
  auto operator()(Args&&... args) {
    return T(std::forward<Args>(args)...);
  }
};

} // anonymous namespace

template <template <typename...> class Ctor>
auto dir_entry_view_impl::make_dir_entry_view(uint32_t self_index,
                                              uint32_t parent_index,
                                              global_metadata const& g,
                                              entry_name_type name_type) {
  auto meta = g.meta();

  if (auto de = meta->dir_entries()) {
    DWARFS_CHECK(self_index < de->size(),
                 fmt::format("self_index out of range: {} >= {}", self_index, de->size()));
    DWARFS_CHECK(parent_index < de->size(),
                 fmt::format("parent_index out of range: {} >= {}", parent_index, de->size()));

    auto dev = de->Get(self_index);
    return Ctor<dir_entry_view_impl>()(dev, self_index, parent_index, g, name_type);
  }

  DWARFS_CHECK(self_index < meta->inodes()->size(),
               fmt::format("self_index out of range: {} >= {}", self_index, meta->inodes()->size()));
  DWARFS_CHECK(parent_index < meta->inodes()->size(),
               fmt::format("parent_index out of range: {} >= {}", parent_index, meta->inodes()->size()));

  auto iv = meta->inodes()->Get(self_index);
  return Ctor<dir_entry_view_impl>()(iv, self_index, parent_index, g, name_type);
}

template <template <typename...> class Ctor>
auto dir_entry_view_impl::make_dir_entry_view(uint32_t self_index,
                                              global_metadata const& g,
                                              entry_name_type name_type) {
  auto meta = g.meta();

  if (auto de = meta->dir_entries()) {
    DWARFS_CHECK(self_index < de->size(),
                 fmt::format("self_index out of range: {} >= {}", self_index, de->size()));
    auto dev = de->Get(self_index);
    DWARFS_CHECK(dev->inode_num() < meta->directories()->size(),
                 fmt::format("inode_num out of range: {} >= {}", dev->inode_num(), meta->directories()->size()));
    return Ctor<dir_entry_view_impl>()(
        dev, self_index, g.parent_dir_entry(dev->inode_num()), g, name_type);
  }

  DWARFS_CHECK(self_index < meta->inodes()->size(),
               fmt::format("self_index out of range: {} >= {}", self_index, meta->inodes()->size()));
  auto iv = meta->inodes()->Get(self_index);

  auto iv_v22 = iv->inode_v2_2();
  DWARFS_CHECK(iv_v22 < meta->directories()->size(),
               fmt::format("inode_v2_2 out of range: {} >= {}", iv_v22, meta->directories()->size()));

  auto parent_dir = meta->directories()->Get(iv_v22);
  auto entry_table = meta->entry_table_v2_2();
  uint32_t parent_idx = entry_table ? entry_table->Get(parent_dir->parent_entry()) : parent_dir->parent_entry();

  return Ctor<dir_entry_view_impl>()(iv, self_index, parent_idx, g, name_type);
}

std::shared_ptr<dir_entry_view_impl>
dir_entry_view_impl::from_dir_entry_index_shared(
    uint32_t self_index, uint32_t parent_index, global_metadata const& g,
    entry_name_type name_type) {
  return make_dir_entry_view<shared_ptr_ctor>(self_index, parent_index, g, name_type);
}

std::shared_ptr<dir_entry_view_impl>
dir_entry_view_impl::from_dir_entry_index_shared(
    uint32_t self_index, global_metadata const& g,
    entry_name_type name_type) {
  return make_dir_entry_view<shared_ptr_ctor>(self_index, g, name_type);
}

dir_entry_view_impl
dir_entry_view_impl::from_dir_entry_index(
    uint32_t self_index, uint32_t parent_index, global_metadata const& g,
    entry_name_type name_type) {
  return make_dir_entry_view<stack_ctor>(self_index, parent_index, g, name_type);
}

std::string dir_entry_view_impl::name(uint32_t index, global_metadata const& g) {
  auto meta = g.meta();
  auto names = meta->names();
  return names && index < names->size() ? names->Get(index)->str() : "";
}

std::string dir_entry_view_impl::name() const {
  switch (g_.get_data()) {
  case entry_name_type::other:
    break;
  case entry_name_type::self:
    return ".";
  case entry_name_type::parent:
    return "..";
  }

  if (is_root()) {
    return {};
  }

  // Comprehensive validation before string table access
  auto const& names = g_->names();
  auto const names_size = names.size();  // Use size() for entry count, not unpacked_size() (bytes)
  
  if (names_size == 0) {
    std::cerr << "ERROR: dir_entry_view_impl::name(): String table is empty!" << std::endl;
    return "";
  }

  // Use variant to handle both DirEntry and InodeData
  // IMPORTANT: Use g_->names() string_table, not meta->names() which may be null
  // when compact_names (FSST compression) is used
  if (std::holds_alternative<::dwarfs::flatbuffers::DirEntry const*>(v_)) {
    auto dev = std::get<::dwarfs::flatbuffers::DirEntry const*>(v_);
    auto name_idx = dev->name_index();
    
    // Bounds check BEFORE access
    if (name_idx >= names_size) {
      std::cerr << "ERROR: dir_entry_view_impl::name(): DirEntry name_index " << name_idx
                << " out of range (size=" << names_size << ", inode=" << dev->inode_num() << ")" << std::endl;
      return "";
    }
    
    auto name_str = names[name_idx];
    
    if (name_str.empty() && !is_root()) {
      std::cerr << "WARNING: Empty name for DirEntry with name_index=" << name_idx
                << ", inode=" << dev->inode_num() << std::endl;
    }
    
    return name_str;
  } else {
    auto iv = std::get<::dwarfs::flatbuffers::InodeData const*>(v_);
    auto name_idx = iv->name_index_v2_2();
    
    // Bounds check BEFORE access
    if (name_idx >= names_size) {
      std::cerr << "ERROR: dir_entry_view_impl::name(): InodeData name_index " << name_idx
                << " out of range (size=" << names_size << ")" << std::endl;
      return "";
    }
    
    auto name_str = names[name_idx];
    
    if (name_str.empty() && !is_root()) {
      std::cerr << "WARNING: Empty name for InodeData with name_index=" << name_idx << std::endl;
    }
    
    return name_str;
  }
}

template <template <typename...> class Ctor>
auto dir_entry_view_impl::make_inode() const {
  if (std::holds_alternative<::dwarfs::flatbuffers::DirEntry const*>(v_)) {
    auto dev = std::get<::dwarfs::flatbuffers::DirEntry const*>(v_);
    auto meta = g_->meta();
    auto inodes = meta->inodes();
    auto inode_data = inodes ? inodes->Get(dev->inode_num()) : nullptr;
    // FIX: Use unary plus to convert lvalue to prvalue, avoiding perfect forwarding deducing uint32_t&
    return Ctor<inode_view_impl>()(inode_data, +dev->inode_num(), meta);
  } else {
    auto iv = std::get<::dwarfs::flatbuffers::InodeData const*>(v_);
    auto v22 = iv->inode_v2_2();
    // FIX: Use unary plus on the ternary result to force prvalue conversion
    return Ctor<inode_view_impl>()(iv, +(v22 != 0 ? v22 : self_index_), g_->meta());
  }
}

std::shared_ptr<inode_view_impl> dir_entry_view_impl::inode_shared_concrete() const {
  return make_inode<shared_ptr_ctor>();
}

std::shared_ptr<inode_view_interface const> dir_entry_view_impl::inode_shared() const {
  // Return interface pointer (implements override)
  return inode_shared_concrete();
}

std::shared_ptr<inode_view_interface> dir_entry_view_impl::inode() const {
  // Return shared_ptr to match interface signature
  return inode_shared_concrete();
}

inode_view_impl dir_entry_view_impl::inode_concrete() const {
  return make_inode<stack_ctor>();
}

bool dir_entry_view_impl::is_root() const {
  if (std::holds_alternative<::dwarfs::flatbuffers::DirEntry const*>(v_)) {
    auto dev = std::get<::dwarfs::flatbuffers::DirEntry const*>(v_);
    return dev->inode_num() == 0;
  } else {
    auto iv = std::get<::dwarfs::flatbuffers::InodeData const*>(v_);
    auto v22 = iv->inode_v2_2();
    return (v22 != 0 ? v22 : self_index_) == 0;
  }
}

std::unique_ptr<dir_entry_view_interface> dir_entry_view_impl::parent() const {
  if (is_root()) {
    return nullptr;
  }

  // Create shared_ptr of concrete type, then convert to unique_ptr of interface
  auto concrete = from_dir_entry_index_shared(parent_index_, *g_);
  if (!concrete) {
    return nullptr;
  }
  // Move the concrete object into a unique_ptr to interface
  return std::make_unique<dir_entry_view_impl>(std::move(*concrete));
}

std::string dir_entry_view_impl::path() const {
  // paths in metadata are guaranteed to be valid UTF-8
  return u8string_to_string(fs_path().u8string());
}

std::string dir_entry_view_impl::unix_path() const {
  static constexpr char preferred =
      static_cast<char>(std::filesystem::path::preferred_separator);
  auto p = path();
  if constexpr (preferred != '/') {
    std::ranges::replace(p, preferred, '/');
  }
  return p;
}

std::filesystem::path dir_entry_view_impl::fs_path() const {
  std::filesystem::path p;
  append_to(p);
  return p;
}

std::wstring dir_entry_view_impl::wpath() const {
  return fs_path().wstring();
}

void dir_entry_view_impl::append_to(std::filesystem::path& p) const {
  if (auto ev = parent_shared()) {
    if (!ev->is_root()) {
      ev->append_to(p);
    }
  }
  if (!is_root()) {
    p /= string_to_u8string(name());
  }
}

std::shared_ptr<dir_entry_view_impl> dir_entry_view_impl::parent_shared() const {
  if (is_root()) {
    return nullptr;
  }
  return from_dir_entry_index_shared(parent_index_, *g_);
}

//==============================================================================
// chunk_view implementation
//==============================================================================

chunk_view::chunk_view(Meta meta, ::dwarfs::flatbuffers::Chunk const* chunk) {
  if (!chunk || !meta) {
    block_ = 0;
    offset_ = 0;
    bits_ = 0;
    return;
  }

  auto const b = chunk->block();
  auto const o = chunk->offset();
  auto const s = chunk->size();
  auto const hole_ix = meta->hole_block_index();

  if (hole_ix != 0 && b == hole_ix) { // this is a hole
    block_ = 0;
    offset_ = 0;
    if (o == kChunkOffsetIsLargeHole) {
      auto large_holes = meta->large_hole_size();
      assert(large_holes != nullptr);
      assert(s < large_holes->size());
      bits_ = large_holes->Get(s);
    } else {
      auto const block_size = meta->block_size();
      assert(std::has_single_bit(block_size));
      assert(o < block_size);
      bits_ = (static_cast<uint64_t>(s) * block_size) + o;
    }
    bits_ |= kChunkBitsHoleBit;
  } else { // this is data
    block_ = b;
    offset_ = o;
    bits_ = s;
  }
}

//==============================================================================
// chunk_range::iterator implementation
//==============================================================================

chunk_view const& chunk_range::iterator::operator*() const {
  if (meta_) {
    auto chunks = meta_->chunks();
    if (chunks && it_ < chunks->size()) {
      view_ = chunk_view(meta_, chunks->Get(it_));
      return view_;
    }
  }
  view_ = chunk_view(nullptr, nullptr);
  return view_;
}

chunk_view const* chunk_range::iterator::operator->() const {
  return &(**this);
}

} // namespace dwarfs::reader::internal::flatbuffers_backend

//==============================================================================
// time_resolution_handler implementation (FlatBuffers version)
//==============================================================================

#ifdef DWARFS_HAVE_FLATBUFFERS

namespace dwarfs::reader::internal {

time_resolution_handler::time_resolution_handler(::dwarfs::flatbuffers::Metadata const* meta)
    : timebase_{meta && meta->timestamp_base() ? meta->timestamp_base() : 0}
    , resolution_{[&]() -> uint32_t {
        if (meta && meta->options()) {
          auto res = meta->options()->time_resolution_sec();
          return res > 0 ? res : 1;
        }
        return 1;
      }()}
    , nsec_multiplier_{[&]() -> uint32_t {
        if (meta && meta->options()) {
          return meta->options()->subsecond_resolution_nsec_multiplier();
        }
        return 0;
      }()}
    , mtime_only_{meta && meta->options() ? meta->options()->mtime_only() : false} {
}

time_resolution_handler::time_resolution_handler(::dwarfs::flatbuffers::HistoryEntry const* hist)
    : timebase_{0}  // TODO: HistoryEntry may not have timestamp_base field
    , resolution_{[&]() -> uint32_t {
        if (hist && hist->options()) {
          auto res = hist->options()->time_resolution_sec();
          return res > 0 ? res : 1;
        }
        return 1;
      }()}
    , nsec_multiplier_{[&]() -> uint32_t {
        if (hist && hist->options()) {
          return hist->options()->subsecond_resolution_nsec_multiplier();
        }
        return 0;
      }()}
    , mtime_only_{hist && hist->options() ? hist->options()->mtime_only() : false} {
}

template <typename T>
time_resolution_handler::time_resolution_handler(T const* obj, uint64_t timebase)
    : timebase_{timebase}
    , resolution_{[&]() -> uint32_t {
        if (obj && obj->options()) {
          auto res = obj->options()->time_resolution_sec();
          return res > 0 ? res : 1;
        }
        return 1;
      }()}
    , nsec_multiplier_{[&]() -> uint32_t {
        if (obj && obj->options()) {
          return obj->options()->subsecond_resolution_nsec_multiplier();
        }
        return 0;
      }()}
    , mtime_only_{obj && obj->options() ? obj->options()->mtime_only() : false} {
}

void time_resolution_handler::fill_stat_timevals(file_stat& st, flatbuffers_backend::inode_view_impl const& ivr) const {
  // FlatBuffers implementation matching Thrift pattern
  auto const mtime_nsec =
      nsec_multiplier_ > 0 ? ivr.mtime_subsec() * nsec_multiplier_ : 0;
  st.set_mtimespec(resolution_ * (timebase_ + ivr.mtime_offset()), mtime_nsec);

  if (mtime_only_) {
    st.set_atimespec(st.mtimespec_unchecked());
    st.set_ctimespec(st.mtimespec_unchecked());
  } else {
    auto const atime_nsec =
        nsec_multiplier_ > 0 ? ivr.atime_subsec() * nsec_multiplier_ : 0;
    auto const ctime_nsec =
        nsec_multiplier_ > 0 ? ivr.ctime_subsec() * nsec_multiplier_ : 0;
    st.set_atimespec(resolution_ * (timebase_ + ivr.atime_offset()),
                     atime_nsec);
    st.set_ctimespec(resolution_ * (timebase_ + ivr.ctime_offset()),
                     ctime_nsec);
  }
}

void time_resolution_handler::add_time_resolution_to(nlohmann::json& j) const {
  j["time_resolution"] = get_time_resolution_string();
  if (nsec_multiplier_ > 0) {
    j["subsecond_resolution_nsec_multiplier"] = nsec_multiplier_;
  }
  if (timebase_ > 0) {
    j["timestamp_base"] = timebase_;
  }
}

std::string time_resolution_handler::get_time_resolution_string() const {
  if (resolution_ == 1) {
    return "1s";
  }
  if (resolution_ < 60) {
    return std::to_string(resolution_) + "s";
  }
  if (resolution_ < 3600 && (resolution_ % 60) == 0) {
    return std::to_string(resolution_ / 60) + "m";
  }
  if ((resolution_ % 3600) == 0) {
    return std::to_string(resolution_ / 3600) + "h";
  }
  return std::to_string(resolution_) + "s";
}

} // namespace dwarfs::reader::internal

#endif // DWARFS_HAVE_FLATBUFFERS
