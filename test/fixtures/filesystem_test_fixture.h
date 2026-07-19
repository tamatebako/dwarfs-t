#pragma once

#include <memory>
#include <string>

#include <dwarfs/reader/filesystem_v2.h>

#include "dwarfs_test_fixture.h"

namespace dwarfs {

namespace test {

// Filesystem-specific fixture providing helpers for filesystem testing
// Inherits common setup from DwarfsTestFixture
class FilesystemTestFixture : public DwarfsTestFixture {
protected:
  // Filesystem-specific setup (extends base setup)
  void SetUp() override;

  // Filesystem-specific helpers
  void verify_file(std::string const& path,
                   std::string const& expected_content);

  void verify_uid_gid(std::string const& path,
                      uint32_t expected_uid,
                      uint32_t expected_gid);

  void verify_access(std::string const& path,
                     int mode,
                     uint32_t uid,
                     uint32_t gid,
                     bool expected_result);

  // Helper to create filesystem from image (stores file_view properly)
  void create_filesystem_from_image(std::string const& fsimage);

  // Filesystem-specific state
  file_view file_view_;  // Must outlive filesystem_
  std::unique_ptr<reader::filesystem_v2> filesystem_;
};

}  // namespace test
}  // namespace dwarfs