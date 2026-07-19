/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \author     Ribose (@riboseinc @tamatebako)
 * \copyright  Copyright (c) Ribose
 */
#include <gtest/gtest.h>
#include "dwarfs/metadata/serialization/serialization_facade.h"
#include "dwarfs/metadata/serialization/facade_factory.h"
#include "dwarfs/metadata/serialization/serialization_format.h"
#include "dwarfs/metadata/domain/metadata.h"

using namespace dwarfs::metadata;

namespace {

} // anonymous namespace

// NOTE: Serialization facade tests are minimal with current architecture:
// - FlatBuffers: Modern default format (can write)
// - Thrift: Legacy format (read-only for backward compatibility)
//
// The facade pattern is primarily used for format abstraction during
// recompression workflows. Most serialization testing happens through
// the main serialization_test.cpp file and integration tests.

// Test: Unknown format detection
TEST(FacadeFactoryTest, DetectUnknownFormat) {
  std::vector<uint8_t> unknown_data = {0xFF, 0xFF, 0xFF, 0xFF};

  auto detected = serialization::FacadeFactory::detect_format(unknown_data);

  EXPECT_FALSE(detected.has_value());

  // create_from_data should return nullptr
  auto facade = serialization::FacadeFactory::create_from_data(unknown_data);
  EXPECT_EQ(facade, nullptr);
}

// Placeholder test to ensure the test file compiles
TEST(SerializationFacadeTest, PlaceholderTest) {
  // This test exists to maintain the test file structure
  // Actual facade tests should be added when testing specific
  // serialization workflows
  EXPECT_TRUE(true);
}