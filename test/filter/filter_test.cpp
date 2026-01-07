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

#include <sstream>
#include <unordered_set>

#include <gtest/gtest.h>

#include <dwarfs/block_compressor.h>
#include <dwarfs/file_type.h>
#include <dwarfs/logger.h>
#include <dwarfs/reader/filesystem_options.h>
#include <dwarfs/reader/filesystem_v2.h>
#include <dwarfs/thread_pool.h>
#include <dwarfs/writer/entry_factory.h>
#include <dwarfs/writer/filesystem_writer.h>
#include <dwarfs/writer/filter_debug.h>
#include <dwarfs/writer/rule_based_entry_filter.h>
#include <dwarfs/writer/scanner.h>
#include <dwarfs/writer/scanner_options.h>
#include <dwarfs/writer/segmenter_factory.h>
#include <dwarfs/writer/writer_progress.h>

#include "../filter_test_data.h"
#include "../loremipsum.h"
#include "../mmap_mock.h"
#include "../test_common.h"
#include "../test_helpers.h"
#include "../test_logger.h"

using namespace dwarfs;

class filter_test
    : public testing::TestWithParam<dwarfs::test::filter_test_data> {
 public:
  test::test_logger lgr;
  std::unique_ptr<writer::rule_based_entry_filter> rbf;
  std::shared_ptr<test::test_file_access> tfa;
  std::shared_ptr<test::os_access_mock> input;

  void SetUp() override {
    tfa = std::make_shared<test::test_file_access>();

    rbf = std::make_unique<writer::rule_based_entry_filter>(lgr, tfa);
    rbf->set_root_path("");

    input = std::make_shared<test::os_access_mock>();

    for (auto const& [stat, name] : dwarfs::test::test_dirtree()) {
      auto path = name.substr(name.size() == 5 ? 5 : 6);

      switch (stat.type()) {
      case posix_file_type::regular:
        input->add(path, stat,
                   [size = stat.size] { return test::loremipsum(size); });
        break;
      case posix_file_type::symlink:
        input->add(path, stat, test::loremipsum(stat.size));
        break;
      default:
        input->add(path, stat);
        break;
      }
    }
  }

  void set_filter_rules(test::filter_test_data const& spec) {
    std::istringstream iss(spec.filter());
    rbf->add_rules(iss);
  }

  std::string get_filter_debug_output(test::filter_test_data const& spec,
                                      writer::debug_filter_mode mode) {
    set_filter_rules(spec);

    std::ostringstream oss;

    writer::scanner_options options;
    options.remove_empty_dirs = false;
    options.debug_filter_function = [&](bool exclude,
                                        writer::entry_interface const& ei) {
      debug_filter_output(oss, exclude, ei, mode);
    };

    writer::writer_progress prog;
    thread_pool pool(lgr, *input, "worker", 1);
    writer::segmenter_factory sf(lgr, prog);
    writer::entry_factory ef;
    writer::scanner s(lgr, pool, sf, ef, *input, options);

    s.add_filter(std::move(rbf));

    block_compressor bc("null");
    std::ostringstream null;
    writer::filesystem_writer fsw(null, lgr, pool, prog);
    fsw.add_default_compressor(bc);
    s.scan(fsw, std::filesystem::path("/"), prog);

    return oss.str();
  }

  void TearDown() override {
    rbf.reset();
    input.reset();
    tfa.reset();
  }
};

TEST_P(filter_test, filesystem) {
  auto spec = GetParam();

  set_filter_rules(spec);

  writer::segmenter::config cfg;

  writer::scanner_options options;
  options.remove_empty_dirs = true;

  auto fsimage = test::build_dwarfs(lgr, input, "null", cfg, options, {}, nullptr,
                              nullptr, std::nullopt, std::move(rbf));

  auto mm = test::make_mock_file_view(std::move(fsimage));

  reader::filesystem_options opts;
  opts.block_cache.max_bytes = 1 << 20;
  opts.metadata.check_consistency = true;

  reader::filesystem_v2 fs(lgr, *input, mm, opts);

  std::unordered_set<std::string> got;

  fs.walk([&got](reader::dir_entry_view e) { got.emplace(e.unix_path()); });

  EXPECT_EQ(spec.expected_files(), got);
}

TEST_P(filter_test, debug_filter_function_included) {
  auto spec = GetParam();
  auto output =
      get_filter_debug_output(spec, writer::debug_filter_mode::INCLUDED);
  auto expected =
      spec.get_expected_filter_output(writer::debug_filter_mode::INCLUDED);
  EXPECT_EQ(expected, output);
}

TEST_P(filter_test, debug_filter_function_included_files) {
  auto spec = GetParam();
  auto output =
      get_filter_debug_output(spec, writer::debug_filter_mode::INCLUDED_FILES);
  auto expected = spec.get_expected_filter_output(
      writer::debug_filter_mode::INCLUDED_FILES);
  EXPECT_EQ(expected, output);
}

TEST_P(filter_test, debug_filter_function_excluded) {
  auto spec = GetParam();
  auto output =
      get_filter_debug_output(spec, writer::debug_filter_mode::EXCLUDED);
  auto expected =
      spec.get_expected_filter_output(writer::debug_filter_mode::EXCLUDED);
  EXPECT_EQ(expected, output);
}

TEST_P(filter_test, debug_filter_function_excluded_files) {
  auto spec = GetParam();
  auto output =
      get_filter_debug_output(spec, writer::debug_filter_mode::EXCLUDED_FILES);
  auto expected = spec.get_expected_filter_output(
      writer::debug_filter_mode::EXCLUDED_FILES);
  EXPECT_EQ(expected, output);
}

TEST_P(filter_test, debug_filter_function_all) {
  auto spec = GetParam();
  auto output = get_filter_debug_output(spec, writer::debug_filter_mode::ALL);
  auto expected =
      spec.get_expected_filter_output(writer::debug_filter_mode::ALL);
  EXPECT_EQ(expected, output);
}

TEST_P(filter_test, debug_filter_function_files) {
  auto spec = GetParam();
  auto output = get_filter_debug_output(spec, writer::debug_filter_mode::FILES);
  auto expected =
      spec.get_expected_filter_output(writer::debug_filter_mode::FILES);
  EXPECT_EQ(expected, output);
}

INSTANTIATE_TEST_SUITE_P(dwarfs, filter_test,
                         ::testing::ValuesIn(dwarfs::test::get_filter_tests()));