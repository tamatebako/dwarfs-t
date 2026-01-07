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
#include "dwarfs/metadata/domain/metadata.h"
#include "dwarfs/metadata/modern/domain_to_thrift.h"
#include "dwarfs/metadata/modern/thrift_to_domain.h"
#include "metadata_modern_types.h"

using namespace dwarfs::metadata;

namespace {

// Helper functions using domain model
domain::metadata create_test_metadata() {
  domain::metadata meta;
  meta.block_size = 4096;
  meta.total_fs_size = 1024 * 1024;
  meta.timestamp_base = 1609459200;
  // Minimal for placeholder
  return meta;
}

void verify_metadata_equal(
    const domain::metadata& expected,
    const domain::metadata& actual) {
  EXPECT_EQ(expected.block_size, actual.block_size);
  EXPECT_EQ(expected.total_fs_size, actual.total_fs_size);
  EXPECT_EQ(expected.timestamp_base, actual.timestamp_base);
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
