#pragma once

#include <memory>
#include <optional>
#include <span>
#include <string>

#include <gtest/gtest.h>

#include <dwarfs/writer/scanner_options.h>
#include <dwarfs/writer/segmenter_factory.h>
#include <dwarfs/writer/writer_progress.h>

#include "../test_logger.h"
#include "../test_helpers.h"
#include "../mmap_mock.h"

namespace dwarfs {

namespace reader {
class filesystem_v2;
}  // namespace reader

namespace test {

// Abstract base fixture providing common setup and factory methods
// All DwarFS tests should inherit from this
class DwarfsTestFixture : public ::testing::Test {
protected:
  // Template Method Pattern: SetUp/TearDown
  void SetUp() override;
  void TearDown() override;

  // Factory Methods (Factory Pattern)
  virtual std::shared_ptr<os_access_mock> create_input();
  virtual writer::scanner_options create_scanner_options();
  virtual writer::segmenter::config create_segmenter_config();

  // Helper to create pre-populated test instance
  std::shared_ptr<os_access_mock> create_test_instance();

  // Builder Methods (Builder Pattern)
  // Build a filesystem image from input with specified compression
  std::string build_filesystem(
      std::string const& compression = "null",
      writer::segmenter::config const& cfg = {},
      writer::scanner_options const& options = {});

  // Create a file view from filesystem image data
  file_view create_file_view(std::string fsimage);

  // Create a reader from a file view
  std::unique_ptr<reader::filesystem_v2> create_reader(
      std::string const& fsimage);

  // Format detection helpers (added in Session 10 for cross-format testing)
  static bool has_flatbuffers() {
#ifdef DWARFS_HAVE_FLATBUFFERS
    return true;
#else
    return false;
#endif
  }

  static bool has_thrift() {
#ifdef DWARFS_HAVE_EXPERIMENTAL_THRIFT
    return true;
#else
    return false;
#endif
  }

  // Common state (shared across all tests)
  test_logger logger_;
  std::shared_ptr<os_access_mock> input_;
  writer::writer_progress progress_;
};

}  // namespace test
}  // namespace dwarfs