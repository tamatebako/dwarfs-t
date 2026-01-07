/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \file
 * \brief HTTP server wrapper using libmicrohttpd
 *
 * Provides a simple HTTP server for serving static files from DwarFS.
 */

#pragma once

#include <memory>
#include <string>

#include <microhttpd.h>

namespace static_site_server {

class dwarfs_loader; // Forward declaration

/**
 * \brief HTTP server configuration
 */
struct http_server_config {
  unsigned short port{8080};     ///< Port to bind (default: 8080)
  std::string bind_address{""}; ///< Bind address (empty = all interfaces)
  bool verbose{false};           ///< Enable verbose logging
};

/**
 * \brief HTTP server using libmicrohttpd
 *
 * Simple HTTP server that serves files from a DwarFS image via
 * the dwarfs_loader interface. Supports:
 * - GET requests for static files
 * - Automatic MIME type detection
 * - 404 error pages
 * - Clean shutdown
 *
 * Example:
 * \code
 * auto loader = dwarfs_loader::create(dwarfs_config);
 *
 * http_server_config config;
 * config.port = 8080;
 *
 * http_server server(loader.get(), config);
 * server.start();
 *
 * // ... server is running ...
 *
 * server.stop(); // Clean shutdown
 * \endcode
 */
class http_server {
public:
  /**
   * \brief Construct HTTP server
   *
   * \param loader DwarFS loader to serve files from
   * \param config Server configuration
   *
   * The loader must remain valid for the lifetime of the server.
   */
  explicit http_server(dwarfs_loader* loader, http_server_config config);

  /**
   * \brief Destructor - stops server if running
   */
  ~http_server();

  // Non-copyable, non-movable (libmicrohttpd daemon is not movable)
  http_server(http_server const&) = delete;
  http_server& operator=(http_server const&) = delete;
  http_server(http_server&&) = delete;
  http_server& operator=(http_server&&) = delete;

  /**
   * \brief Start the HTTP server
   *
   * \return true if server started successfully
   *
   * Starts the server and begins accepting connections.
   * Non-blocking - returns immediately.
   */
  bool start();

  /**
   * \brief Stop the HTTP server
   *
   * Gracefully stops the server and closes all connections.
   */
  void stop();

  /**
   * \brief Check if server is running
   *
   * \return true if server is currently running
   */
  bool is_running() const;

  /**
   * \brief Get server URL
   *
   * \return URL of running server (e.g., "http://127.0.0.1:8080")
   */
  std::string get_url() const;

private:
  dwarfs_loader* loader_;          ///< DwarFS loader (not owned)
  http_server_config config_;      ///< Server configuration
  MHD_Daemon* daemon_;             ///< libmicrohttpd daemon handle

  /**
   * \brief libmicrohttpd callback (static) - returns MHD_Result
   */
  static MHD_Result handle_request_static(
      void* cls, MHD_Connection* connection, char const* url,
      char const* method, char const* version, char const* upload_data,
      size_t* upload_data_size, void** con_cls);

  /**
   * \brief Handle HTTP request (member function) - returns MHD_Result
   */
  MHD_Result handle_request(MHD_Connection* connection, char const* url,
                     char const* method);

  /**
   * \brief Send file response - returns MHD_Result
   */
  MHD_Result send_file_response(MHD_Connection* connection,
                         std::string const& content,
                         std::string const& mime_type);

  /**
   * \brief Send error response (404, 500, etc.) - returns MHD_Result
   */
  MHD_Result send_error_response(MHD_Connection* connection,
                          unsigned int status_code,
                          char const* message);
};

} // namespace static_site_server
