/* vim:set ts=2 sw=2 sts=2 et: */

#include "dwarfs_loader.h"

#include <algorithm>
#include <iostream>
#include <sstream>
#include <system_error>
#include <unordered_map>

#include <dwarfs/logger.h>
#include <dwarfs/os_access_generic.h>
#include <dwarfs/reader/filesystem_loader.h>
#include <dwarfs/reader/filesystem_v2.h>

namespace static_site_server {

namespace {

/**
 * \brief MIME type mapping by file extension
 */
std::unordered_map<std::string_view, std::string_view> const mime_types = {
    {".html", "text/html"},
    {".htm", "text/html"},
    {".css", "text/css"},
    {".js", "text/javascript"},
    {".json", "application/json"},
    {".xml", "application/xml"},
    {".txt", "text/plain"},
    {".md", "text/markdown"},

    // Images
    {".jpg", "image/jpeg"},
    {".jpeg", "image/jpeg"},
    {".png", "image/png"},
    {".gif", "image/gif"},
    {".svg", "image/svg+xml"},
    {".ico", "image/x-icon"},
    {".webp", "image/webp"},

    // Fonts
    {".woff", "font/woff"},
    {".woff2", "font/woff2"},
    {".ttf", "font/ttf"},
    {".otf", "font/otf"},

    // Other
    {".pdf", "application/pdf"},
    {".zip", "application/zip"},
};

/**
 * \brief Extract file extension from path
 */
std::string_view get_extension(std::string_view path) {
  auto pos = path.find_last_of('.');
  if (pos == std::string_view::npos) {
    return "";
  }
  return path.substr(pos);
}

} // anonymous namespace

/**
 * \brief PIMPL implementation
 */
struct dwarfs_loader::impl {
  std::unique_ptr<dwarfs::logger> logger;
  std::unique_ptr<dwarfs::os_access> os;
  dwarfs::reader::filesystem_v2_lite fs;

  impl(dwarfs::reader::filesystem_v2_lite&& filesystem,
       std::unique_ptr<dwarfs::logger> lgr,
       std::unique_ptr<dwarfs::os_access> os_access)
      : logger(std::move(lgr))
      , os(std::move(os_access))
      , fs(std::move(filesystem)) {}
};

// Static factory method
std::unique_ptr<dwarfs_loader>
dwarfs_loader::create(dwarfs_loader_config const& config) {
  try {
    // Create logger (suppress warnings during normal operation)
    dwarfs::logger_options log_opts;
    log_opts.threshold = dwarfs::logger::ERROR;
    auto lgr = std::make_unique<dwarfs::stream_logger>(log_opts);

    // Create OS access layer
    auto os = std::make_unique<dwarfs::os_access_generic>();

    // Configure filesystem loader
    dwarfs::reader::filesystem_load_config fs_config;
    fs_config.image_path = config.image_path;
    fs_config.cache_size = config.cache_size;
    fs_config.num_workers = config.num_workers;

    // Load filesystem
    auto fs = dwarfs::reader::filesystem_loader::load(*lgr, *os, fs_config);

    // Create implementation with moved resources
    auto impl_ptr = std::make_unique<impl>(
        std::move(fs), std::move(lgr), std::move(os));

    // Use placement new to construct dwarfs_loader with private constructor
    auto loader = std::unique_ptr<dwarfs_loader>(new dwarfs_loader(config));
    loader->impl_ = std::move(impl_ptr);

    return loader;

  } catch (std::exception const& e) {
    std::cerr << "Failed to load DwarFS image: " << e.what() << "\n";
    return nullptr;
  }
}

// Private constructor
dwarfs_loader::dwarfs_loader(dwarfs_loader_config const& /*config*/)
    : impl_(nullptr) {
  // impl_ is set by create() factory
}

// Destructor
dwarfs_loader::~dwarfs_loader() = default;

// Move constructor
dwarfs_loader::dwarfs_loader(dwarfs_loader&& other) noexcept = default;

// Move assignment
dwarfs_loader& dwarfs_loader::operator=(dwarfs_loader&& other) noexcept =
    default;

// Get file content
std::optional<file_content>
dwarfs_loader::get_file(std::string_view path) const {
  if (!impl_) {
    return std::nullopt;
  }

  try {
    std::error_code ec;

    // Find file in filesystem
    auto entry_opt = impl_->fs.find(std::string(path));
    if (!entry_opt) {
      return std::nullopt; // Not found
    }

    auto inode = entry_opt->inode();

    // Get file attributes
    auto stat = impl_->fs.getattr(inode, ec);
    if (ec || !stat.is_regular_file()) {
      return std::nullopt; // Not a regular file or stat failed
    }

    // Open file
    auto inode_num = impl_->fs.open(inode, ec);
    if (ec) {
      return std::nullopt;
    }

    // Read entire file
    auto data = impl_->fs.read_string(inode_num, ec);
    if (ec) {
      return std::nullopt;
    }

    // Prepare result
    file_content result;
    result.data = std::move(data);
    result.size = stat.size();
    result.mime_type = get_mime_type(path);

    return result;

  } catch (std::exception const&) {
    return std::nullopt;
  }
}

// Check if file exists
bool dwarfs_loader::file_exists(std::string_view path) const {
  if (!impl_) {
    return false;
  }

  try {
    auto entry_opt = impl_->fs.find(std::string(path));
    if (!entry_opt) {
      return false;
    }

    std::error_code ec;
    auto stat = impl_->fs.getattr(entry_opt->inode(), ec);

    return !ec && stat.is_regular_file();

  } catch (std::exception const&) {
    return false;
  }
}

// Get MIME type (static)
std::string dwarfs_loader::get_mime_type(std::string_view path) {
  auto ext = get_extension(path);

  // Convert to lowercase for case-insensitive matching
  std::string ext_lower(ext);
  std::transform(ext_lower.begin(), ext_lower.end(), ext_lower.begin(),
                 [](unsigned char c) { return std::tolower(c); });

  auto it = mime_types.find(ext_lower);
  if (it != mime_types.end()) {
    return std::string(it->second);
  }

  return "application/octet-stream"; // Default for unknown
}

// Get filesystem info
std::string dwarfs_loader::get_info() const {
  if (!impl_) {
    return "No filesystem loaded";
  }

  try {
    std::ostringstream oss;

    // Count files by walking filesystem
    size_t file_count = 0;
    size_t total_size = 0;

    impl_->fs.walk([&](auto const& entry) {
      std::error_code ec;
      auto stat = impl_->fs.getattr(entry.inode(), ec);
      if (!ec && stat.is_regular_file()) {
        ++file_count;
        total_size += stat.size();
      }
    });

    oss << file_count << " files, ";

    // Format size
    if (total_size < 1024) {
      oss << total_size << " bytes";
    } else if (total_size < (1024 * 1024)) {
      oss << (total_size / 1024.0) << " KB";
    } else {
      oss << (total_size / (1024.0 * 1024.0)) << " MB";
    }

    return oss.str();

  } catch (std::exception const&) {
    return "Error retrieving filesystem info";
  }
}

// List all files
std::vector<file_info> dwarfs_loader::list_files() const {
  std::vector<file_info> files;

  if (!impl_) {
    return files;
  }

  try {
    impl_->fs.walk([&](auto const& entry) {
      std::error_code ec;
      auto stat = impl_->fs.getattr(entry.inode(), ec);
      if (!ec && stat.is_regular_file()) {
        file_info info;
        info.path = entry.path();
        info.size = stat.size();
        files.push_back(std::move(info));
      }
    });
  } catch (std::exception const&) {
    // Return whatever we collected so far
  }

  return files;
}

} // namespace static_site_server
