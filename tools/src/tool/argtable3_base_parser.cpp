//
// Copyright (c) Marcus Holland-Moritz
//
// This file is part of dwarfs.
//
// dwarfs is free software: you can redistribute it and/or modify it under the
// terms of the GNU General Public License as published by the Free Software
// Foundation, either version 3 of the License, or (at your option) any later
// version.
//
// dwarfs is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
// A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along with
// dwarfs.  If not, see <https://www.gnu.org/licenses/>.
//

#include <dwarfs/config.h>  // For DWARFS_BUILTIN_MANPAGE
#include "dwarfs/tool/argtable3_base_parser.h"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <vector>

#include <argtable3.h>
#include <fmt/format.h>

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>  // For CommandLineToArgvW
#endif

#include "dwarfs/tool/version_info.h"
#include "dwarfs/tool/tool.h"
#include "dwarfs/tool/iolayer.h"

#ifdef DWARFS_BUILTIN_MANPAGE
#include "dwarfs/tool/render_manpage.h"
#endif

namespace dwarfs::tool {

argtable3_base_parser::argtable3_base_parser() = default;

argtable3_base_parser::~argtable3_base_parser() {
  // Free argtable memory
  if (!argtable_.empty()) {
    arg_freetable(argtable_.data(), argtable_.size());
  }
}

#ifdef _WIN32
int argtable3_base_parser::parse(int argc, sys_char** argv) {
  // Convert wchar_t** to char** for argtable3
  std::vector<char*> narrow_argv;
  std::vector<std::string> narrow_strings;

  narrow_argv.reserve(argc);
  narrow_strings.reserve(argc);

  for (int i = 0; i < argc; ++i) {
    // Convert wide string to narrow string (UTF-8)
    int size = WideCharToMultiByte(CP_UTF8, 0, argv[i], -1, nullptr, 0,
                                   nullptr, nullptr);
    if (size <= 0) {
      // Conversion failed, use empty string
      narrow_strings.emplace_back();
    } else {
      narrow_strings.emplace_back(size - 1, '\0');
      WideCharToMultiByte(CP_UTF8, 0, argv[i], -1,
                         &narrow_strings.back()[0], size,
                         nullptr, nullptr);
    }
    narrow_argv.push_back(narrow_strings.back().data());
  }

  // Call the virtual parse() with char**
  return parse(argc, narrow_argv.data());
}
#endif

void argtable3_base_parser::init_argtable(size_t max_options) {
  // Reserve space for options (max_options + end)
  argtable_.reserve(max_options + 1);
}

void argtable3_base_parser::add_common_options() {
  // --help, -h
  help_opt_ = arg_litn("h", "help", 0, 1, "display this help and exit");
  argtable_.push_back(help_opt_);

  // --version, -V
  version_opt_ = arg_litn("V", "version", 0, 1, "display version and exit");
  argtable_.push_back(version_opt_);

#ifdef DWARFS_BUILTIN_MANPAGE
  // --man
  man_opt_ = arg_litn(nullptr, "man", 0, 1, "display manual page and exit");
  argtable_.push_back(man_opt_);
#endif
}

void argtable3_base_parser::add_logger_options() {
  // --log-level=LEVEL
  log_level_opt_ = arg_strn("L", "log-level", "LEVEL", 0, 1,
    "set logging level (error|warn|info|verbose|debug|trace)");
  argtable_.push_back(log_level_opt_);

  // --verbose, -v (can be repeated)
  verbose_opt_ = arg_litn("v", "verbose", 0, 2, "increase verbosity");
  argtable_.push_back(verbose_opt_);

  // --quiet, -q
  quiet_opt_ = arg_litn("q", "quiet", 0, 1, "suppress non-error messages");
  argtable_.push_back(quiet_opt_);
}

void argtable3_base_parser::finalize_argtable() {
  // Add error handling struct at end
  end_opt_ = arg_end(20); // max 20 errors
  argtable_.push_back(end_opt_);
}

int argtable3_base_parser::parse_args(int argc, char** argv) {
  // Parse arguments
  int nerrors = arg_parse(argc, argv, argtable_.data());

  // Check for errors first
  if (nerrors > 0) {
    display_errors(argv[0]);
    return 2; // Parse error
  }

  // Check for help/version/man BEFORE validation
  if (help_opt_ && help_opt_->count > 0) {
    help_requested_ = true;
    display_help(argv[0]);
    return 1;
  }

  if (version_opt_ && version_opt_->count > 0) {
    version_requested_ = true;
    display_version();
    return 1;
  }

#ifdef DWARFS_BUILTIN_MANPAGE
  if (man_opt_ && man_opt_->count > 0) {
    man_requested_ = true;
    display_manpage();
    return 1;
  }
#endif

  // Update logger options from parsed values
  update_logger_options();

  return 0; // Success
}

void argtable3_base_parser::update_logger_options() {
  // Priority: --quiet > --log-level > --verbose

  if (quiet_opt_ && quiet_opt_->count > 0) {
    logger_opts_.threshold = LOGGER_LEVEL_ERROR;
    return;
  }

  if (log_level_opt_ && log_level_opt_->count > 0) {
    try {
      logger_opts_.threshold = logger::parse_level(log_level_opt_->sval[0]);
    } catch (std::exception const& e) {
      std::cerr << "Invalid log level: " << log_level_opt_->sval[0] << "\n";
      std::cerr << "Valid levels: " << logger::all_level_names() << "\n";
      // Keep default
    }
    return;
  }

  if (verbose_opt_ && verbose_opt_->count > 0) {
    // -v = INFO, -vv = VERBOSE
    if (verbose_opt_->count == 1) {
      logger_opts_.threshold = LOGGER_LEVEL_INFO;
    } else if (verbose_opt_->count >= 2) {
      logger_opts_.threshold = LOGGER_LEVEL_VERBOSE;
    }
    return;
  }

  // Default is WARN (already set in logger_options)
}

void argtable3_base_parser::display_version() const {
  auto info = version_info::get();
  std::cout << info.to_string(get_tool_name()) << std::flush;
}

void argtable3_base_parser::display_help(std::string_view progname) {
  // Print usage line
  std::cout << "Usage: " << progname;
  arg_print_syntax(stdout, argtable_.data(), "\n");

  // Print glossary
  std::cout << "\n";
  arg_print_glossary(stdout, argtable_.data(), "  %-30s %s\n");

  std::cout << std::flush;
}

void argtable3_base_parser::set_manpage_context(manpage::document const& doc,
                                                  iolayer const& iol) {
  manpage_doc_ = &doc;
  iolayer_ = &iol;
}

void argtable3_base_parser::display_manpage() {
#ifdef DWARFS_BUILTIN_MANPAGE
  if (manpage_doc_ && iolayer_) {
    show_manpage(*manpage_doc_, *iolayer_);
  } else {
    std::cerr << "Manpage not configured. Call set_manpage_context() before parse().\n";
  }
#else
  std::cerr << "Manpage support not built in\n";
#endif
}

void argtable3_base_parser::display_errors(std::string_view progname) {
  if (end_opt_ && end_opt_->count > 0) {
    std::cerr << progname << ": ";
    arg_print_errors(stderr, end_opt_, progname.data());
    std::cerr << "Try '" << progname << " --help' for more information.\n";
  }
}

std::string argtable3_base_parser::get_env_var(std::string_view var_name) const {
  std::string full_name = fmt::format("DWARFS_{}{}",
    env_prefix_.empty() ? "" : env_prefix_ + "_",
    var_name);

  char const* value = std::getenv(full_name.c_str());
  return value ? value : "";
}

bool argtable3_base_parser::has_env_var(std::string_view var_name) const {
  std::string full_name = fmt::format("DWARFS_{}{}",
    env_prefix_.empty() ? "" : env_prefix_ + "_",
    var_name);

  return std::getenv(full_name.c_str()) != nullptr;
}

void argtable3_base_parser::load_environment_variables(std::string_view tool_prefix) {
  env_prefix_ = tool_prefix;

  // Load common environment variables
  if (has_env_var("LOG_LEVEL")) {
    auto level_str = get_env_var("LOG_LEVEL");
    try {
      logger_opts_.threshold = logger::parse_level(level_str);
    } catch (...) {
      // Ignore invalid values in environment
    }
  }

  // Subclasses can override to load tool-specific environment variables
}

} // namespace dwarfs::tool