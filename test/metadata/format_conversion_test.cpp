/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \author     Ribose (@riboseinc @tamatebako)
 * \copyright  Copyright (c) Ribose
 */
#include <gtest/gtest.h>

#ifdef DWARFS_HAVE_THRIFT

#include "dwarfs/metadata/serialization/serialization_facade.h"
#include "dwarfs/metadata/serialization/facade_factory.h"
#include "dwarfs/metadata/serialization/serialization_format.h"
#include "thrift/dwarfs/gen-cpp2/metadata_types.h"

using namespace dwarfs::metadata;

namespace {

// Helper functions using Thrift types - kept inside guard
::dwarfs::thrift::metadata::metadata create_test_metadata() {
  ::dwarfs::thrift::metadata::metadata meta;
  meta.block_size() = 4096;
  meta.total_fs_size() = 1024 * 1024;
  meta.timestamp_base() = 1609459200;
  // Minimal for placeholder
  return meta;
}

void verify_metadata_equal(
    const ::dwarfs::thrift::metadata::metadata& expected,
    const ::dwarfs::thrift::metadata::metadata& actual) {
  EXPECT_EQ(expected.block_size(), actual.block_size());
  EXPECT_EQ(expected.total_fs_size(), actual.total_fs_size());
  EXPECT_EQ(expected.timestamp_base(), actual.timestamp_base());
}

} // anonymous namespace

// Placeholder test
TEST(FormatConversionTest, PlaceholderTest) {
  EXPECT_TRUE(true);
}

#else

TEST(FormatConversionTest, thrift_unavailable) {
  GTEST_SKIP() << "Thrift not enabled - skipping format conversion tests";
}

#endif // DWARFS_HAVE_THRIFT
