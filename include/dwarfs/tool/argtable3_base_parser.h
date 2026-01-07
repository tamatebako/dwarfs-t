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

#pragma once

#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <dwarfs/logger.h>

// Forward declare types
namespace dwarfs::tool {
struct iolayer;
namespace manpage {
struct line;
using document = std::span<line const>;
} // namespace manpage
} // namespace dwarfs::tool

// Forward declare argtable3 types
struct arg_lit;
struct arg_str;
struct arg_int;
struct arg_dbl;
struct arg_file;
struct arg_end;

namespace dwarfs::tool {

class version_info;

/**
 * @brief Base class for argtable3-based option parsers
 *
 * Provides common option handling infrastructure for all DwarFS tools.
 * This class uses the Template Method pattern - subclasses implement
 * define_tool_options() to add tool-specific options.
 */
class argtable3_base_parser {
public:
  virtual ~argtable3_base_parser();

  /**
   * @brief Set manpage context for --man flag
   * @param doc Manpage document to display
   * @param iol I/O layer for display
   *
   * Must be called before parse() to enable --man functionality.
   */
  void set_manpage_context(manpage::document const& doc, iolayer const& iol);

  /**
   * @brief Parse command-line arguments
   * @param argc Argument count
   * @param argv Argument vector
   * @return 0 on success, 1 if help/version shown, 2 on parse error
   */
  virtual int parse(int argc, char** argv) = 0;

  /**
   * @brief Check if help was requested
   */
  bool help() const { return help_requested_; }

  /**
   * @brief Check if version was requested
   */
  bool version() const { return version_requested_; }

  /**
   * @brief Check if manpage was requested
   */
  bool man() const { return man_requested_; }

  /**
   * @brief Get logger options from parsed arguments
   */
  logger_options const& get_logger_options() const { return logger_opts_; }

  /**
   * @brief Load environment variables for this tool
   * @param tool_prefix Tool-specific prefix (e.g., "MKDWARFS")
   */
  void load_environment_variables(std::string_view tool_prefix);

protected:
  argtable3_base_parser();

  /**
   * @brief Add common options (--help, --version, --man)
   * Subclasses MUST call this in their define_tool_options() implementation
   */
  void add_common_options();

  /**
   * @brief Add logger options (--log-level, --verbose, etc.)
   * Subclasses MAY call this if they want logger options
   */
  void add_logger_options();

  /**
   * @brief Define tool-specific options
   * Subclasses implement this to add their specific command-line options.
   * This is called during parse() before argument parsing.
   */
  virtual void define_tool_options() = 0;

  /**
   * @brief Validate parsed options
   * Subclasses implement this to perform validation after parsing.
   * @return true if validation passed, false otherwise
   */
  virtual bool validate_options() = 0;

  /**
   * @brief Get tool name for version display
   */
  virtual std::string_view get_tool_name() const = 0;

  /**
   * @brief Initialize argtable
   * Must be called before adding any options
   */
  void init_argtable(size_t max_options);

  /**
   * @brief Finalize argtable
   * Must be called after all options are added
   */
  void finalize_argtable();

  /**
   * @brief Parse arguments using argtable3
   * @param argc Argument count
   * @param argv Argument vector
   * @return Number of parsing errors (0 = success)
   */
  int parse_args(int argc, char** argv);

  /**
   * @brief Display version information
   */
  void display_version() const;

  /**
   * @brief Display help text
   * @param progname Program name
   */
  void display_help(std::string_view progname);

  /**
   * @brief Display manpage
   */
  void display_manpage();

  /**
   * @brief Get environment variable value
   * @param var_name Variable name (without DWARFS_ prefix)
   * @return Value if set, empty string otherwise
   */
  std::string get_env_var(std::string_view var_name) const;

  /**
   * @brief Check if environment variable is set
   */
  bool has_env_var(std::string_view var_name) const;

  // Common option storage (protected so subclasses can access)
  struct arg_lit* help_opt_{nullptr};
  struct arg_lit* version_opt_{nullptr};
  struct arg_lit* man_opt_{nullptr};

  // Logger option storage
  struct arg_str* log_level_opt_{nullptr};
  struct arg_lit* verbose_opt_{nullptr};
  struct arg_lit* quiet_opt_{nullptr};

  // Argtable storage
  std::vector<void*> argtable_;
  struct arg_end* end_opt_{nullptr};

  // Parsed state
  bool help_requested_{false};
  bool version_requested_{false};
  bool man_requested_{false};
  logger_options logger_opts_;

  // Environment variable prefix (set by load_environment_variables)
  std::string env_prefix_;

  // Manpage context (set via set_manpage_context)
  manpage::document const* manpage_doc_{nullptr};
  iolayer const* iolayer_{nullptr};
private:
  /**
   * @brief Update logger options from parsed argtable values
   */
  void update_logger_options();

  /**
   * @brief Display error messages from argtable parsing
   */
  void display_errors(std::string_view progname);
};

} // namespace dwarfs::tool