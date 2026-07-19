/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \file
 * \brief Static site webserver serving content from DwarFS image
 *
 * Example application demonstrating libdwarfs C++ API integration
 * with libmicrohttpd to serve static HTML content.
 */

#include <csignal>
#include <iostream>
#include <memory>
#include <thread>

#include <boost/program_options.hpp>

#include "dwarfs_loader.h"
#include "http_server.h"

namespace po = boost::program_options;
using namespace static_site_server;

namespace {

// Global server pointer for signal handler
std::unique_ptr<http_server> g_server;

/**
 * \brief Signal handler for clean shutdown
 */
void signal_handler(int signal) {
  if (signal == SIGINT || signal == SIGTERM) {
    std::cout << "\nShutting down...\n";
    if (g_server) {
      g_server->stop();
    }
  }
}

/**
 * \brief Print usage banner
 */
void print_banner() {
  std::cout << "DwarFS Static Site Server v0.1\n"
            << "Serving static content from DwarFS archives\n\n";
}

} // anonymous namespace

int main(int argc, char** argv) {
  try {
    // Parse command line options
    po::options_description desc("Options");
    desc.add_options()
        ("help,h", "Show this help message")
        ("image,i", po::value<std::string>()->required(),
         "Path to DwarFS image file")
        ("port,p", po::value<unsigned short>()->default_value(8080),
         "Port to bind (default: 8080)")
        ("bind,b", po::value<std::string>()->default_value(""),
         "Bind address (default: all interfaces)")
        ("cache,c", po::value<size_t>()->default_value(128),
         "Cache size in MiB (default: 128)")
        ("workers,w", po::value<size_t>()->default_value(4),
         "Number of worker threads (default: 4)")
        ("verbose,v", "Enable verbose logging");

    po::variables_map vm;

    try {
      po::store(po::parse_command_line(argc, argv, desc), vm);

      if (vm.count("help")) {
        print_banner();
        std::cout << "Usage: " << argv[0] << " --image IMAGE [OPTIONS]\n\n";
        std::cout << desc << "\n";
        std::cout << "Example:\n";
        std::cout << "  " << argv[0] << " --image candide.dff\n";
        std::cout << "  " << argv[0] << " --image site.dff --port 9090 --cache 256 --verbose\n";
        return 0;
      }

      po::notify(vm);
    } catch (po::error const& e) {
      std::cerr << "Error: " << e.what() << "\n\n";
      std::cerr << "Usage: " << argv[0] << " --image IMAGE [OPTIONS]\n";
      std::cerr << "Try '" << argv[0] << " --help' for more information.\n";
      return 1;
    }

    // Extract options
    std::string image_path = vm["image"].as<std::string>();
    unsigned short port = vm["port"].as<unsigned short>();
    std::string bind_addr = vm["bind"].as<std::string>();
    size_t cache_mib = vm["cache"].as<size_t>();
    size_t workers = vm["workers"].as<size_t>();
    bool verbose = vm.count("verbose") > 0;

    if (verbose) {
      print_banner();
    }

    // Load DwarFS image
    if (verbose) {
      std::cout << "Loading DwarFS image: " << image_path << "\n";
    }

    dwarfs_loader_config loader_config;
    loader_config.image_path = image_path;
    loader_config.cache_size = cache_mib << 20; // Convert MiB to bytes
    loader_config.num_workers = workers;

    auto loader = dwarfs_loader::create(loader_config);
    if (!loader) {
      std::cerr << "ERROR: Failed to load DwarFS image\n";
      return 1;
    }

    // Display filesystem info
    auto fs_info = loader->get_info();
    std::cout << "Loaded: " << fs_info << "\n";

    // List first 10 files with sizes
    auto files = loader->list_files();
    if (!files.empty()) {
      size_t count = std::min(files.size(), size_t(10));
      std::cout << "Files (showing " << count << " of " << files.size() << "):\n";
      for (size_t i = 0; i < count; ++i) {
        std::cout << "  " << files[i].path;

        // Format size
        size_t size = files[i].size;
        if (size < 1024) {
          std::cout << " (" << size << " B)\n";
        } else if (size < (1024 * 1024)) {
          std::cout << " (" << (size / 1024.0) << " KB)\n";
        } else {
          std::cout << " (" << (size / (1024.0 * 1024.0)) << " MB)\n";
        }
      }
      if (files.size() > 10) {
        std::cout << "  ... and " << (files.size() - 10) << " more\n";
      }
      std::cout << "\n";
    }

    if (verbose) {
      std::cout << "Cache size: " << cache_mib << " MiB\n";
      std::cout << "Worker threads: " << workers << "\n";
    }

    // Setup HTTP server
    http_server_config server_config;
    server_config.port = port;
    server_config.bind_address = bind_addr;
    server_config.verbose = verbose;

    g_server = std::make_unique<http_server>(loader.get(), server_config);

    // Setup signal handlers
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    // Start server
    if (!g_server->start()) {
      std::cerr << "ERROR: Failed to start HTTP server\n";
      return 1;
    }

    // Keep running until signal received
    while (g_server->is_running()) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return 0;

  } catch (std::exception const& e) {
    std::cerr << "ERROR: " << e.what() << "\n";
    return 1;
  }
}