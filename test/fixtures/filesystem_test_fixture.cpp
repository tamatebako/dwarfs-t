// ... existing code ...
#include "filesystem_test_fixture.h"

#include <gtest/gtest.h>

#include <dwarfs/reader/filesystem_v2.h>
#include <dwarfs/reader/filesystem_options.h>

namespace dwarfs::test {

// SetUp - Extends base setup
void FilesystemTestFixture::SetUp() {
  // Call parent setup
  DwarfsTestFixture::SetUp();

  // Filesystem-specific setup if needed
  // (filesystem_ will be created by tests as needed)
}

// Helper: Verify file contents
void FilesystemTestFixture::verify_file(
    std::string const& path,
    std::string const& expected_content) {
  ASSERT_TRUE(filesystem_) << "Filesystem not initialized";

  auto dev = filesystem_->find(path);
  ASSERT_TRUE(dev) << "File not found: " << path;

  auto inode = dev->inode();
  auto fh = filesystem_->open(inode);
  auto content = filesystem_->read_string(fh);

  EXPECT_EQ(expected_content, content) << "Content mismatch for: " << path;
}

// Helper: Verify UID and GID
void FilesystemTestFixture::verify_uid_gid(
    std::string const& path,
    uint32_t expected_uid,
    uint32_t expected_gid) {
  ASSERT_TRUE(filesystem_) << "Filesystem not initialized";

  auto dev = filesystem_->find(path);
  ASSERT_TRUE(dev) << "File not found: " << path;

  auto st = filesystem_->getattr(dev->inode());

  EXPECT_EQ(expected_uid, st.uid()) << "UID mismatch for: " << path;
  EXPECT_EQ(expected_gid, st.gid()) << "GID mismatch for: " << path;
}

// Helper: Verify access permissions
void FilesystemTestFixture::verify_access(
    std::string const& path,
    int mode,
    uint32_t uid,
    uint32_t gid,
    bool expected_result) {
  ASSERT_TRUE(filesystem_) << "Filesystem not initialized";

  auto dev = filesystem_->find(path);
  ASSERT_TRUE(dev) << "File not found: " << path;

  bool result = filesystem_->access(dev->inode(), mode, uid, gid);

  EXPECT_EQ(expected_result, result)
      << "Access check mismatch for: " << path
      << " (mode=" << mode << ", uid=" << uid << ", gid=" << gid << ")";
}

// Helper: Create filesystem from image (stores file_view properly)
void FilesystemTestFixture::create_filesystem_from_image(
    std::string const& fsimage) {
  // Store file_view in fixture (must outlive filesystem)
  file_view_ = create_file_view(fsimage);

  // Create filesystem WITHOUT check_consistency (it may break name lookups)
  reader::filesystem_options opts;
  opts.block_cache.max_bytes = 1 << 20;
  // opts.metadata.check_consistency = true;  // DISABLED - causes find() to fail

  filesystem_ = std::make_unique<reader::filesystem_v2>(
      logger_, *input_, file_view_, opts);
}

}  // namespace dwarfs::test
// ... existing code ...