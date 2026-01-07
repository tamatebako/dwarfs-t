/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \file
 * \brief DwarFS filesystem wrapper for HTTP server
 *
 * Provides a clean interface to libdwarfs reader API for serving static files.
 */

#pragma once

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// Forward declarations to avoid exposing dwarfs headers
namespace dwarfs {
class logger;
class os_access;

namespace reader {
class filesystem_v2_lite;
} // namespace reader
} // namespace dwarfs

namespace static_site_server {

/**
 * \brief Configuration for DwarFS loader
 */
struct dwarfs_loader_config {
  std::string image_path;           ///< Path to DwarFS image file
  size_t cache_size{128 << 20};     ///< Block cache size (default: 128 MiB)
  size_t num_workers{4};            ///< Number of decompression workers
};

/**
 * \brief File content from DwarFS image
 */
struct file_content {
  std::string data;       ///< File content bytes
  std::string mime_type;  ///< MIME type based on file extension
  size_t size;            ///< File size in bytes
};

/**
 * \brief File information (for directory listings)
 */
struct file_info {
  std::string path;  ///< File path
  size_t size;       ///< File size in bytes
};

/**
 * \brief Wrapper for DwarFS filesystem providing file access
 *
 * This class wraps the libdwarfs reader API and provides a simple
 * interface for reading files from a DwarFS image. It handles:
 * - Image loading and initialization
 * - File lookup and reading
 * - MIME type detection by extension
 * - Error handling via std::optional
 *
 * Example:
 * \code
 * dwarfs_loader_config config;
 * config.image_path = "site.dff";
 *
 * auto loader = dwarfs_loader::create(config);
 * if (auto content = loader->get_file("/index.html")) {
 *   std::cout << "Size: " << content->size << " bytes\n";
 *   std::cout << "MIME: " << content->mime_type << "\n";
 * }
 * \endcode
 */
class dwarfs_loader {
public:
  /**
   * \brief Create DwarFS loader
   *
   * \param config Configuration with image path and options
   * \return Unique pointer to loader, or nullptr on failure
   *
   * This factory method loads the DwarFS image and initializes
   * the filesystem. Returns nullptr if loading fails.
   */
  static std::unique_ptr<dwarfs_loader> create(
      dwarfs_loader_config const& config);

  /**
   * \brief Destructor - RAII cleanup
   */
  ~dwarfs_loader();

  // Non-copyable, movable
  dwarfs_loader(dwarfs_loader const&) = delete;
  dwarfs_loader& operator=(dwarfs_loader const&) = delete;
  dwarfs_loader(dwarfs_loader&&) noexcept;
  dwarfs_loader& operator=(dwarfs_loader&&) noexcept;

  /**
   * \brief Read file from DwarFS image
   *
   * \param path Absolute path within image (e.g., "/index.html")
   * \return File content, or std::nullopt if not found or on error
   *
   * Reads the entire file into memory. For large files, consider
   * implementing streaming reads.
   */
  std::optional<file_content> get_file(std::string_view path) const;

  /**
   * \brief Check if file exists in image
   *
   * \param path Absolute path within image
   * \return true if file exists and is a regular file
   */
  bool file_exists(std::string_view path) const;

  /**
   * \brief Get MIME type for file extension
   *
   * \param path File path
   * \return MIME type string (e.g., "text/html")
   *
   * Returns "application/octet-stream" for unknown extensions.
   */
  static std::string get_mime_type(std::string_view path);

  /**
   * \brief Get filesystem statistics
   *
   * \return Human-readable filesystem info
   */
  std::string get_info() const;

  /**
   * \brief List all files in filesystem
   *
   * \return Vector of file information (path and size)
   */
  std::vector<file_info> list_files() const;

private:
  /**
   * \brief Private constructor - use create() factory
   */
  explicit dwarfs_loader(dwarfs_loader_config const& config);

  struct impl; ///< PIMPL idiom to hide implementation
  std::unique_ptr<impl> impl_; ///< Implementation pointer
};

} // namespace static_site_server