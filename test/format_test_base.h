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

#pragma once

#include <string>
#include <vector>

#include <gtest/gtest.h>

namespace dwarfs::test {

/**
 * Test parameter for format selection
 * 
 * Used to parameterize tests that can work with different metadata formats
 * (FlatBuffers, Thrift, or both).
 */
struct format_param {
  std::string name;        ///< Format name for test naming ("flatbuffers", "thrift", "both")
  bool has_thrift;         ///< Whether Thrift support is enabled
  bool has_flatbuffers;    ///< Whether FlatBuffers support is enabled
};

/**
 * Get list of available formats based on compile-time configuration
 * 
 * Returns a vector of format_param instances for all formats available
 * in the current build configuration. This is used by INSTANTIATE_FORMAT_TESTS
 * to create test instances for each available format.
 * 
 * @return Vector of available format parameters
 */
inline std::vector<format_param> get_available_formats() {
  std::vector<format_param> formats;
  
#ifdef DWARFS_HAVE_FLATBUFFERS
  formats.push_back({"flatbuffers", false, true});
#endif

#ifdef DWARFS_HAVE_THRIFT
  formats.push_back({"thrift", true, false});
#endif

#if defined(DWARFS_HAVE_THRIFT) && defined(DWARFS_HAVE_FLATBUFFERS)
  formats.push_back({"both", true, true});
#endif

  return formats;
}

/**
 * Macro to define a format-parameterized test
 * 
 * Creates a test class that inherits from TestWithParam<format_param>.
 * Use GetParam() inside the test to access the format configuration.
 * 
 * Example:
 * @code
 * DWARFS_FORMAT_TEST(filesystem, read) {
 *   auto const& format = GetParam();
 *   
 *   // Skip if format not available
 *   if (format.has_thrift && !defined(DWARFS_HAVE_THRIFT)) {
 *     GTEST_SKIP() << "Thrift not available in this build";
 *   }
 *   
 *   // Create filesystem with specified format
 *   auto fs = create_test_filesystem(format);
 *   
 *   // Test logic (format-agnostic)
 *   // ...
 * }
 * INSTANTIATE_FORMAT_TESTS(filesystem, read);
 * @endcode
 */
#define DWARFS_FORMAT_TEST(test_suite, test_name)                             \
  class test_suite##_##test_name##_Format                                     \
      : public ::testing::TestWithParam<dwarfs::test::format_param> {};       \
  TEST_P(test_suite##_##test_name##_Format, test_name)

/**
 * Macro to instantiate format-parameterized tests
 * 
 * Must be called after DWARFS_FORMAT_TEST to actually create test instances.
 * Creates one test instance for each available format (as determined by
 * get_available_formats()).
 * 
 * The test names will be suffixed with "/flatbuffers", "/thrift", or "/both"
 * depending on the format configuration.
 */
#define INSTANTIATE_FORMAT_TESTS(test_suite, test_name)                       \
  INSTANTIATE_TEST_SUITE_P(                                                   \
      Formats, test_suite##_##test_name##_Format,                             \
      ::testing::ValuesIn(dwarfs::test::get_available_formats()),             \
      [](auto const& info) { return info.param.name; })

} // namespace dwarfs::test