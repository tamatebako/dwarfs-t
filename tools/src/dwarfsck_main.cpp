/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \author     Marcus Holland-Moritz (github@mhxnet.de)
 * \copyright  Copyright (c) Marcus Holland-Moritz
 *
 * This file is part of dwarfs.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * SPDX-License-Identifier: MIT
 */

#include <algorithm>
#include <cstring>
#include <iostream>
#include <mutex>
#include <string_view>
#include <vector>

#include <fmt/chrono.h>
#include <fmt/format.h>
#if FMT_VERSION >= 110000
#include <fmt/ranges.h>
#endif

#include <dwarfs/checksum.h>
#include <dwarfs/config.h>
#include <dwarfs/conv.h>
#include <dwarfs/counting_semaphore.h>
#include <dwarfs/decompressor_registry.h>
#include <dwarfs/error.h>
#include <dwarfs/file_access.h>
#include <dwarfs/logger.h>
#include <dwarfs/os_access.h>
#include <dwarfs/reader/detail/file_reader.h>
#include <dwarfs/reader/filesystem_options.h>
#include <dwarfs/reader/filesystem_v2.h>
#include <dwarfs/reader/fsinfo_options.h>
#include <dwarfs/thread_pool.h>
#include <dwarfs/tool/dwarfsck/argtable3_options_parser.h>
#include <dwarfs/tool/iolayer.h>
#include <dwarfs/tool/tool.h>
#include <dwarfs/util.h>
#include <dwarfs_tool_main.h>
#include <dwarfs_tool_manpage.h>

namespace dwarfs::tool {

namespace {

void do_list_files(reader::filesystem_v2& fs, iolayer const& iol,
                   bool verbose) {
  auto max_width = [](auto const& vec) {
    auto max = std::max_element(vec.begin(), vec.end());
    return std::to_string(*max).size();
  };

  auto const uid_width = max_width(fs.get_all_uids());
  auto const gid_width = max_width(fs.get_all_gids());

  size_t inode_size_width{0};

  if (verbose) {
    file_stat::off_type max_inode_size{0};
    fs.walk([&](auto const& de) {
      auto st = fs.getattr(de.inode());
      max_inode_size = std::max(max_inode_size, st.size());
    });
    inode_size_width = fmt::format("{:L}", max_inode_size).size();
  }

  fs.walk([&](auto const& de) {
    auto name = de.unix_path();
    utf8_sanitize(name);

    if (verbose) {
      auto iv = de.inode();

      if (iv.is_symlink()) {
        auto target = fs.readlink(iv);
        utf8_sanitize(target);
        name += " -> " + target;
      }

      auto st = fs.getattr(iv);

      iol.out << fmt::format("{3} {4:{0}}/{5:{1}} {6:{2}L} {7:%F %H:%M} {8}\n",
                             uid_width, gid_width, inode_size_width,
                             iv.mode_string(), iv.getuid(), iv.getgid(),
                             st.size(), safe_localtime(st.mtime()), name);
    } else if (!name.empty()) {
      iol.out << name << "\n";
    }
  });
}

void do_checksum(logger& lgr, reader::filesystem_v2& fs, iolayer const& iol,
                 std::string const& algo, size_t num_workers,
                 size_t max_queued_bytes) {
  LOG_PROXY(debug_logger_policy, lgr);

  std::mutex mx;
  counting_semaphore sem;
  sem.post(static_cast<int64_t>(max_queued_bytes));

  thread_pool pool{lgr, *iol.os, "checksum", num_workers};

  size_t const max_queued_per_worker = max_queued_bytes / num_workers;

  fs.walk_data_order([&](auto const& de) {
    auto iv = de.inode();

    if (iv.is_regular_file()) {
      reader::detail::file_reader fr(fs, iv);

      pool.add_job(
          [&, de,
           ranges = fr.read_sequential(sem, max_queued_per_worker)]() mutable {
            try {
              checksum cs(algo);

              for (auto const& r : ranges) {
                cs.update(r.data(), r.size());
              }

              auto output =
                  fmt::format("{}  {}\n", cs.hexdigest(), de.unix_path());

              {
                std::lock_guard lock(mx);
                iol.out << output;
              }
            } catch (std::exception const& e) {
              LOG_ERROR << "error processing inode for " << de.unix_path()
                        << ": " << e.what();
            }
          });
    }
  });

  pool.wait();
}

} // namespace

int dwarfsck_main(int argc, sys_char** argv, iolayer const& iol) {
  try {
    dwarfsck::argtable3_options_parser opt_parser;

#ifdef DWARFS_BUILTIN_MANPAGE
    // Wire up manpage for --man flag
    opt_parser.set_manpage_context(manpage::get_dwarfsck_manpage(), iol);
#endif

    // Parse arguments
    int parse_result = opt_parser.parse(argc, argv);
    if (parse_result != 0) {
      return parse_result == 1 ? 0 : 1; // 1=help/version shown, 2=error
    }

    // Load environment variables (after CLI parsing, before use)
    opt_parser.load_environment_variables();

    // Get parsed options
    auto const& opts_ = opt_parser.get_parsed_options();

    // Create logger from base parser's logger options
    auto logopts = opt_parser.get_logger_options();
    stream_logger lgr(iol.term, iol.err, logopts);
    LOG_PROXY(debug_logger_policy, lgr);

    // Setup filesystem options
    reader::filesystem_options fsopts;
    fsopts.metadata.check_consistency = !opts_.no_check;
    fsopts.image_offset = reader::parse_image_offset(opts_.image_offset);
    fsopts.block_cache.max_bytes = parse_size_with_unit(opts_.cache_size);
    fsopts.block_cache.num_workers = opts_.num_workers;

    auto input_path = iol.os->canonical(opts_.input);
    auto mm = iol.os->open_file(input_path);

    if (opts_.print_header) {
      if (auto hdr =
              reader::filesystem_v2::header(lgr, mm, fsopts.image_offset)) {
        ensure_binary_mode(iol.out);
        for (auto const& ext : *hdr) {
          for (auto const& seg : ext.segments()) {
            auto const data = seg.span<char>();
            iol.out.write(data.data(), data.size());
          }
        }
        if (iol.out.bad() || iol.out.fail()) {
          LOG_ERROR << "error writing header";
          return 1;
        }
      } else {
        LOG_WARN << "filesystem does not contain a header";
        return 2;
      }
    } else {
      reader::filesystem_v2 fs(lgr, *iol.os, mm, fsopts);

      if (!opts_.export_metadata.empty()) {
        std::error_code ec;
        auto of = iol.file->open_output(iol.os->canonical(opts_.export_metadata), ec);
        if (ec) {
          LOG_ERROR << "failed to open metadata output file: " << ec.message();
          return 1;
        }
        auto json = fs.serialize_metadata_as_json(false);
        of->os().write(json.data(), json.size());
        of->close(ec);
        if (ec) {
          LOG_ERROR << "failed to close metadata output file: " << ec.message();
          return 1;
        }
      } else {
        auto level = opts_.check_integrity
                         ? reader::filesystem_check_level::FULL
                         : reader::filesystem_check_level::CHECKSUM;
        auto errors = opts_.no_check ? 0 : fs.check(level, opts_.num_workers);

        if (!opts_.quiet && !opts_.list_files && opts_.checksum_algo.empty()) {
          reader::fsinfo_options info_opts;

          info_opts.block_access =
              opts_.no_check ? reader::block_access_level::no_verify
                             : reader::block_access_level::unrestricted;

          auto numeric_detail = try_to<int>(opts_.detail);
          info_opts.features =
              numeric_detail.has_value()
                  ? reader::fsinfo_features::for_level(*numeric_detail)
                  : reader::fsinfo_features::parse(opts_.detail);

          if (opts_.output_json) {
            iol.out << fs.info_as_json(info_opts) << "\n";
          } else {
            fs.dump(iol.out, info_opts);
          }
        }

        if (opts_.list_files) {
          do_list_files(fs, iol, opts_.verbose);
        }

        if (!opts_.checksum_algo.empty()) {
          do_checksum(lgr, fs, iol, opts_.checksum_algo, opts_.num_workers,
                      fsopts.block_cache.max_bytes);
        }

        if (errors > 0) {
          return 1;
        }
      }
    }
  } catch (std::exception const& e) {
    iol.err << exception_str(e) << "\n";
    return 1;
  }

  return 0;
}

} // namespace dwarfs::tool