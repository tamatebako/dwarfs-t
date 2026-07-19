/* vim:set ts=2 sw=2 sts=2 et: */

#include "http_server.h"
#include "dwarfs_loader.h"

#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <vector>

#include <microhttpd.h>

namespace static_site_server {

namespace {

/**
 * \brief Generate 404 HTML page
 */
std::string generate_404_page(std::string const& path) {
  std::ostringstream oss;
  oss << "<!DOCTYPE html>\n"
      << "<html>\n"
      << "<head><title>404 Not Found</title></head>\n"
      << "<body>\n"
      << "<h1>404 Not Found</h1>\n"
      << "<p>The requested file <code>" << path << "</code> was not found.</p>\n"
      << "</body>\n"
      << "</html>\n";
  return oss.str();
}

/**
 * \brief Generate 500 HTML page
 */
std::string generate_500_page() {
  return "<!DOCTYPE html>\n"
         "<html>\n"
         "<head><title>500 Internal Server Error</title></head>\n"
         "<body>\n"
         "<h1>500 Internal Server Error</h1>\n"
         "<p>An internal error occurred while processing your request.</p>\n"
         "</body>\n"
         "</html>\n";
}

/**
 * \brief Generate 405 HTML page
 */
std::string generate_405_page(std::string const& method) {
  std::ostringstream oss;
  oss << "<!DOCTYPE html>\n"
      << "<html>\n"
      << "<head><title>405 Method Not Allowed</title></head>\n"
      << "<body>\n"
      << "<h1>405 Method Not Allowed</h1>\n"
      << "<p>Method <code>" << method << "</code> is not allowed.</p>\n"
      << "<p>Only GET requests are supported.</p>\n"
      << "</body>\n"
      << "</html>\n";
  return oss.str();
}

} // anonymous namespace

// Constructor
http_server::http_server(dwarfs_loader* loader, http_server_config config)
    : loader_(loader)
    , config_(std::move(config))
    , daemon_(nullptr) {
  if (!loader_) {
    throw std::invalid_argument("DwarFS loader cannot be null");
  }
}

// Destructor
http_server::~http_server() {
  stop();
}

// Start server
bool http_server::start() {
  if (daemon_) {
    return true; // Already running
  }

  // Start libmicrohttpd daemon
  daemon_ = MHD_start_daemon(
      MHD_USE_SELECT_INTERNALLY,  // Use internal thread for event loop
      config_.port,
      nullptr,                     // Accept all connections
      nullptr,
      &http_server::handle_request_static,
      this,                        // Pass this as callback data
      MHD_OPTION_END);

  if (!daemon_) {
    std::cerr << "Failed to start HTTP server on port " << config_.port << "\n";
    return false;
  }

  // Always print server URL so users know where to connect
  std::cout << "Server started: " << get_url() << "\n";
  std::cout << "Browse to: " << get_url() << "/\n";
  std::cout << "Press Ctrl+C to stop\n\n";

  return true;
}

// Stop server
void http_server::stop() {
  if (daemon_) {
    MHD_stop_daemon(daemon_);
    daemon_ = nullptr;

    if (config_.verbose) {
      std::cout << "Server stopped\n";
    }
  }
}

// Check if running
bool http_server::is_running() const {
  return daemon_ != nullptr;
}

// Get server URL
std::string http_server::get_url() const {
  std::ostringstream oss;
  oss << "http://";

  if (config_.bind_address.empty()) {
    oss << "127.0.0.1";
  } else {
    oss << config_.bind_address;
  }

  oss << ":" << config_.port;
  return oss.str();
}

// Static callback bridge (returns MHD_Result for libmicrohttpd 1.0+)
MHD_Result http_server::handle_request_static(
    void* cls, MHD_Connection* connection, char const* url,
    char const* method, char const* /*version*/,
    char const* /*upload_data*/, size_t* /*upload_data_size*/,
    void** con_cls) {

  // First call for each request - initialize connection state
  if (*con_cls == nullptr) {
    *con_cls = cls; // Mark as initialized
    return MHD_YES;
  }

  // Bridge to member function
  auto* server = static_cast<http_server*>(cls);
  return server->handle_request(connection, url, method);
}

// Handle HTTP request
MHD_Result http_server::handle_request(MHD_Connection* connection,
                                 char const* url, char const* method) {
  // Only support GET
  if (std::strcmp(method, "GET") != 0) {
    auto page = generate_405_page(method);
    return send_error_response(connection, 405, page.c_str());
  }

  std::string path(url);

  // Special route: /ls/ - show file listing
  if (path == "/ls/" || path == "/ls") {
    auto files = loader_->list_files();
    std::ostringstream oss;
    oss << "<!DOCTYPE html>\n"
        << "<html>\n"
        << "<head>\n"
        << "  <title>File Listing</title>\n"
        << "  <style>\n"
        << "    body { font-family: monospace; margin: 2em; }\n"
        << "    table { border-collapse: collapse; width: 100%; }\n"
        << "    th, td { text-align: left; padding: 8px; border-bottom: 1px solid #ddd; }\n"
        << "    th { background-color: #f0f0f0; }\n"
        << "    a { text-decoration: none; color: #0066cc; }\n"
        << "    a:hover { text-decoration: underline; }\n"
        << "    .size { text-align: right; }\n"
        << "  </style>\n"
        << "</head>\n"
        << "<body>\n"
        << "<h1>Files in DwarFS Image</h1>\n"
        << "<p>Total files: " << files.size() << "</p>\n"
        << "<table>\n"
        << "<tr><th>File</th><th class=\"size\">Size</th></tr>\n";

    for (auto const& file : files) {
      oss << "<tr><td><a href=\"" << file.path << "\">" << file.path << "</a></td>";
      oss << "<td class=\"size\">";

      // Format size
      if (file.size < 1024) {
        oss << file.size << " B";
      } else if (file.size < (1024 * 1024)) {
        oss << std::fixed << std::setprecision(2) << (file.size / 1024.0) << " KB";
      } else {
        oss << std::fixed << std::setprecision(2) << (file.size / (1024.0 * 1024.0)) << " MB";
      }

      oss << "</td></tr>\n";
    }

    oss << "</table>\n"
        << "</body>\n"
        << "</html>\n";

    auto listing = oss.str();

    auto* response = MHD_create_response_from_buffer(
        listing.size(),
        const_cast<char*>(listing.data()),
        MHD_RESPMEM_MUST_COPY);

    if (!response) {
      return send_error_response(connection, 500, generate_500_page().c_str());
    }

    MHD_add_response_header(response, "Content-Type", "text/html");
    MHD_Result ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
    MHD_destroy_response(response);

    if (config_.verbose) {
      std::cout << "GET " << url << " -> 200 OK (file listing)\n";
    }

    return ret;
  }

  // Redirect / to an index page
  if (path == "/") {
    // Try common index pages in order
    std::vector<std::string> index_candidates = {
        "/index.html",
        "/pg11339-images.html",  // Aesop's Fables
        "/pg19942-images.html"   // Candide
    };

    for (auto const& candidate : index_candidates) {
      if (loader_->file_exists(candidate)) {
        path = candidate;
        break;
      }
    }

    // If no index found, return 404
    if (path == "/") {
      auto page = generate_404_page("/");
      return send_error_response(connection, 404, page.c_str());
    }
  }

  // Log request if verbose
  if (config_.verbose) {
    std::cout << "GET " << url;
    if (path != url) {
      std::cout << " -> " << path;
    }
  }

  // Try to get file from DwarFS
  auto content_opt = loader_->get_file(path);

  if (!content_opt) {
    if (config_.verbose) {
      std::cout << " -> 404 Not Found\n";
    }

    auto page = generate_404_page(path);
    return send_error_response(connection, 404, page.c_str());
  }

  auto& content = *content_opt;

  if (config_.verbose) {
    std::cout << " -> 200 OK (" << content.size << " bytes)\n";
  }

  return send_file_response(connection, content.data, content.mime_type);
}

// Send file response
MHD_Result http_server::send_file_response(MHD_Connection* connection,
                                     std::string const& content,
                                     std::string const& mime_type) {
  // Create response
  auto* response = MHD_create_response_from_buffer(
      content.size(),
      const_cast<char*>(content.data()),
      MHD_RESPMEM_MUST_COPY);

  if (!response) {
    return send_error_response(connection, 500, generate_500_page().c_str());
  }

  // Set headers
  MHD_add_response_header(response, "Content-Type", mime_type.c_str());
  MHD_add_response_header(response, "Server", "DwarFS-StaticSiteServer/0.1");

  // Send response
  MHD_Result ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
  MHD_destroy_response(response);

  return ret;
}

// Send error response
MHD_Result http_server::send_error_response(MHD_Connection* connection,
                                      unsigned int status_code,
                                      char const* message) {
  auto* response = MHD_create_response_from_buffer(
      std::strlen(message),
      const_cast<char*>(message),
      MHD_RESPMEM_MUST_COPY);

  if (!response) {
    return MHD_NO;
  }

  MHD_add_response_header(response, "Content-Type", "text/html");

  MHD_Result ret = MHD_queue_response(connection, status_code, response);
  MHD_destroy_response(response);

  return ret;
}

} // namespace static_site_server
