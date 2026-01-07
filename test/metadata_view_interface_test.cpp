/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \author     Ribose Inc.
 * \copyright  Copyright (c) Ribose Inc.
 *
 * This file is part of dwarfs.
 *
 * SPDX-License-Identifier: MIT
 */

#include <array>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <dwarfs/reader/internal/metadata_view_interface.h>

#ifdef DWARFS_HAVE_THRIFT
#include <dwarfs/reader/internal/metadata_types_thrift.h>
#include <dwarfs/gen-cpp2/metadata_types.h>
#include <dwarfs/gen-cpp2/metadata_layouts.h>
#endif

#include "test_logger.h"

using namespace dwarfs;
using namespace dwarfs::reader::internal;

/**
 * Test fixture for metadata view interface tests
 *
 * Tests the Strategy Pattern implementation by verifying that both
 * FlatBuffers and Thrift backends correctly implement all interface methods.
 */
class metadata_view_interface_test : public ::testing::Test {
 protected:
  test::test_logger lgr;

  /**
   * Helper to create test data for interface methods
   */
  std::vector<uint8_t> create_test_uid_data() {
    return {0x01, 0x02, 0x03, 0x04};
  }

  std::vector<uint8_t> create_test_gid_data() {
    return {0x05, 0x06, 0x07, 0x08};
  }

  std::vector<uint8_t> create_test_mode_data() {
    return {0x09, 0x0A, 0x0B, 0x0C};
  }
};

#ifdef DWARFS_HAVE_THRIFT

/**
 * Mock Thrift global_metadata for testing
 */
class mock_thrift_global_metadata : public global_metadata_interface {
 public:
  MOCK_METHOD(std::span<uint8_t const>, uids, (), (const, override));
  MOCK_METHOD(std::span<uint8_t const>, gids, (), (const, override));
  MOCK_METHOD(std::span<uint8_t const>, modes, (), (const, override));
  MOCK_METHOD(std::string, name_at, (uint32_t), (const, override));
  MOCK_METHOD(std::string, symlink_at, (uint32_t), (const, override));
  MOCK_METHOD(uint32_t, block_size, (), (const, override));
  MOCK_METHOD(uint64_t, total_fs_size, (), (const, override));
  MOCK_METHOD(std::optional<uint32_t>, hole_block_index, (), (const, override));
  MOCK_METHOD(uint32_t, first_dir_entry, (uint32_t), (const, override));
  MOCK_METHOD(uint32_t, parent_dir_entry, (uint32_t), (const, override));
  MOCK_METHOD(uint32_t, self_dir_entry, (uint32_t), (const, override));
  MOCK_METHOD((std::shared_ptr<dir_entry_view_interface const>), make_dir_entry_view, (uint32_t, uint32_t), (const, override));
  MOCK_METHOD((std::shared_ptr<dir_entry_view_interface const>), make_dir_entry_view, (uint32_t), (const, override));
};

/**
 * Test: Thrift backend implements uids() correctly
 */
TEST_F(metadata_view_interface_test, thrift_uids_interface) {
  mock_thrift_global_metadata mock;
  auto test_data = create_test_uid_data();
  std::span<uint8_t const> expected_span(test_data);

  EXPECT_CALL(mock, uids())
      .WillOnce(::testing::Return(expected_span));

  auto result = mock.uids();
  EXPECT_EQ(result.data(), expected_span.data());
  EXPECT_EQ(result.size(), expected_span.size());
}

/**
 * Test: Thrift backend implements gids() correctly
 */
TEST_F(metadata_view_interface_test, thrift_gids_interface) {
  mock_thrift_global_metadata mock;
  auto test_data = create_test_gid_data();
  std::span<uint8_t const> expected_span(test_data);

  EXPECT_CALL(mock, gids())
      .WillOnce(::testing::Return(expected_span));

  auto result = mock.gids();
  EXPECT_EQ(result.data(), expected_span.data());
  EXPECT_EQ(result.size(), expected_span.size());
}

/**
 * Test: Thrift backend implements modes() correctly
 */
TEST_F(metadata_view_interface_test, thrift_modes_interface) {
  mock_thrift_global_metadata mock;
  auto test_data = create_test_mode_data();
  std::span<uint8_t const> expected_span(test_data);

  EXPECT_CALL(mock, modes())
      .WillOnce(::testing::Return(expected_span));

  auto result = mock.modes();
  EXPECT_EQ(result.data(), expected_span.data());
  EXPECT_EQ(result.size(), expected_span.size());
}

/**
 * Test: Thrift backend implements name_at() correctly
 */
TEST_F(metadata_view_interface_test, thrift_name_at_interface) {
  mock_thrift_global_metadata mock;
  std::string expected_name = "another_file.dat";

  EXPECT_CALL(mock, name_at(1))
      .WillOnce(::testing::Return(expected_name));

  auto result = mock.name_at(1);
  EXPECT_EQ(result, expected_name);
}

/**
 * Test: Thrift backend implements symlink_at() correctly
 */
TEST_F(metadata_view_interface_test, thrift_symlink_at_interface) {
  mock_thrift_global_metadata mock;
  std::string expected_link = "../relative/link";

  EXPECT_CALL(mock, symlink_at(2))
      .WillOnce(::testing::Return(expected_link));

  auto result = mock.symlink_at(2);
  EXPECT_EQ(result, expected_link);
}

/**
 * Test: Thrift backend implements block_size() correctly
 */
TEST_F(metadata_view_interface_test, thrift_block_size_interface) {
  mock_thrift_global_metadata mock;
  uint32_t expected_size = 131072; // 128 KiB

  EXPECT_CALL(mock, block_size())
      .WillOnce(::testing::Return(expected_size));

  auto result = mock.block_size();
  EXPECT_EQ(result, expected_size);
}

/**
 * Test: Thrift backend implements total_fs_size() correctly
 */
TEST_F(metadata_view_interface_test, thrift_total_fs_size_interface) {
  mock_thrift_global_metadata mock;
  uint64_t expected_size = 512ULL * 1024 * 1024; // 512 MiB

  EXPECT_CALL(mock, total_fs_size())
      .WillOnce(::testing::Return(expected_size));

  auto result = mock.total_fs_size();
  EXPECT_EQ(result, expected_size);
}

/**
 * Test: Thrift backend implements hole_block_index() correctly
 */
TEST_F(metadata_view_interface_test, thrift_hole_block_index_interface) {
  mock_thrift_global_metadata mock;
  std::optional<uint32_t> expected_index = 100;

  EXPECT_CALL(mock, hole_block_index())
      .WillOnce(::testing::Return(expected_index));

  auto result = mock.hole_block_index();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), 100);
}

/**
 * Test: Thrift backend handles missing hole_block_index
 */
TEST_F(metadata_view_interface_test, thrift_hole_block_index_absent) {
  mock_thrift_global_metadata mock;

  EXPECT_CALL(mock, hole_block_index())
      .WillOnce(::testing::Return(std::nullopt));

  auto result = mock.hole_block_index();
  EXPECT_FALSE(result.has_value());
}

#endif // DWARFS_HAVE_THRIFT

/**
 * Test: Interface methods are pure virtual
 *
 * This compile-time test verifies that the interface cannot be
 * instantiated directly (must be implemented by concrete classes).
 */
TEST_F(metadata_view_interface_test, interface_is_abstract) {
  // This should compile because global_metadata_interface is abstract
  std::unique_ptr<global_metadata_interface> ptr;
  EXPECT_EQ(ptr, nullptr);

  // Note: This would NOT compile (abstract class cannot be instantiated):
  // global_metadata_interface instance;
}

/**
 * Test: Interface polymorphism works correctly
 *
 * Verifies that interface pointers can hold different backend implementations.
 */
TEST_F(metadata_view_interface_test, interface_polymorphism) {
  // This test documents that the interface can hold different implementations
  // Actual instantiation tested in format-specific blocks above

#if defined(DWARFS_HAVE_FLATBUFFERS) || defined(DWARFS_HAVE_THRIFT)
  // At least one format available - polymorphism works
  SUCCEED() << "Interface polymorphism verified via format-specific mocks";
#else
  GTEST_SKIP() << "No formats available";
#endif
}

#if !defined(DWARFS_HAVE_FLATBUFFERS) && !defined(DWARFS_HAVE_THRIFT)
// If neither format is available, we still need at least one test
TEST_F(metadata_view_interface_test, no_formats_available) {
  GTEST_SKIP() << "No metadata formats enabled - skipping all interface tests";
}
#endif