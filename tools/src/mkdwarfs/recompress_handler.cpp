/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \author     Marcus Holland-Moritz (github@mhxnet.de)
 * \copyright  Copyright (c) Marcus Holland-Moritz
 *
 * This file is part of dwarfs.
 *
 * dwarfs is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * dwarfs is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with dwarfs.  If not, see <https://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifdef DWARFS_HAVE_EXPERIMENTAL_THRIFT

#include <dwarfs/tool/mkdwarfs/recompress_handler.h>

#include <memory>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

#include <boost/algorithm/string.hpp>

#include <dwarfs/error.h>
#include <dwarfs/library_dependencies.h>
#include <dwarfs/logger.h>
#include <dwarfs/reader/filesystem_options.h>
#include <dwarfs/reader/filesystem_v2.h>
#include <dwarfs/tool/iolayer.h>
#include <dwarfs/tool/mkdwarfs/argtable3_options_parser.h>
#include <dwarfs/utility/rewrite_filesystem.h>
#include <dwarfs/utility/rewrite_options.h>
#include <dwarfs/writer/console_writer.h>
#include <dwarfs/writer/filesystem_block_category_resolver.h>
#include <dwarfs/writer/filesystem_writer.h>
#include <dwarfs/writer/writer_progress.h>

namespace dwarfs::tool::mkdwarfs {

// Utility: split string to container
template <typename Container>
Container split_to(std::string_view input, char delimiter) {
  Container result;
  std::string current;
  for (char c : input) {
    if (c == delimiter) {
      if (!current.empty()) {
        result.insert(result.end(), current);
        current.clear();
      }
    } else {
      current += c;
    }
  }
  if (!current.empty()) {
    result.insert(result.end(), current);
  }
  return result;
}

// Utility: convert exception to string
std::string exception_str(std::exception const& e) {
  return std::string(e.what());
}

int recompress_handler::run(
    parsed_options const& opts, iolayer const& iol,
    ::dwarfs::writer::console_writer& console,
    ::dwarfs::writer::writer_progress& prog,
    ::dwarfs::writer::filesystem_writer& fsw,
    [[maybe_unused]] ::dwarfs::writer::rule_based_entry_filter* entry_filter,
    std::function<void(library_dependencies&)> const& extra_deps) {

  stream_logger lgr(iol.term, iol.err);
  LOG_PROXY(prod_logger_policy, lgr);

  // Setup rewrite options from parsed options
  utility::rewrite_options rw_opts;

  std::unordered_map<std::string, unsigned> const modes{
      {"all", 3},
      {"metadata", 2},
      {"block", 1},
      {"none", 0},
  };

  std::string recompress_opts = opts.recompress_opts;

  // Determine recompress mode
  if (recompress_opts.empty()) {
    if (opts.change_block_size) {
      recompress_opts = "all";
    } else if (opts.rebuild_metadata) {
      recompress_opts = "metadata";
    }
  }

  if (auto it = modes.find(recompress_opts); it != modes.end()) {
    rw_opts.recompress_block = it->second & 1;
    rw_opts.recompress_metadata = it->second & 2;
  } else {
    iol.err << "invalid recompress mode: " << recompress_opts << "\n";
    return 1;
  }

  // Parse recompress categories if provided
  if (!opts.recompress_categories.empty()) {
    if (opts.change_block_size) {
      iol.err << "cannot use --recompress-categories with --change-block-size\n";
      return 1;
    }

    std::string_view input = opts.recompress_categories;
    if (input.front() == '!') {
      rw_opts.recompress_categories_exclude = true;
      input.remove_prefix(1);
    }
    rw_opts.recompress_categories =
        split_to<std::unordered_set<std::string>>(input, ',');
  }

  // Load and check input filesystem
  reader::filesystem_v2 input_filesystem(
      lgr, *iol.os, opts.input_path,
      reader::filesystem_options{
          .image_offset = reader::filesystem_options::IMAGE_OFFSET_AUTO});

  LOG_INFO << "checking input filesystem...";

  {
    auto tv = LOG_TIMED_VERBOSE;

    if (auto num_errors =
            input_filesystem.check(reader::filesystem_check_level::CHECKSUM);
        num_errors != 0) {
      LOG_ERROR << "input filesystem is corrupt: detected " << num_errors
                << " error(s)";
      return 1;
    }

    tv << "checked input filesystem";
  }

  // Setup category resolver from input filesystem
  auto cat_resolver = std::make_shared<writer::filesystem_block_category_resolver>(
      input_filesystem.get_all_block_categories());

  // Validate recompress categories exist in input filesystem
  for (auto const& cat : rw_opts.recompress_categories) {
    if (!cat_resolver->category_value(cat)) {
      LOG_ERROR << "no category '" << cat << "' in input filesystem";
      return 1;
    }
  }

  // Setup rebuild metadata and change block size options
  if (opts.rebuild_metadata || opts.change_block_size) {
    rw_opts.rebuild_metadata = opts.scanner_opts.metadata;
  }
  if (opts.change_block_size) {
    rw_opts.change_block_size =
        UINT64_C(1) << opts.segmenter_config.block_size_bits;
  }

  // Execute rewrite
  try {
    utility::rewrite_filesystem(lgr, input_filesystem, fsw, *cat_resolver,
                                rw_opts, extra_deps);
  } catch (dwarfs::error const& e) {
    LOG_ERROR << exception_str(e);
    return 1;
  } catch (std::exception const& e) {
    LOG_ERROR << exception_str(e);
    return 1;
  }

  return 0;
}

} // namespace dwarfs::tool::mkdwarfs

#endif // DWARFS_HAVE_EXPERIMENTAL_THRIFT