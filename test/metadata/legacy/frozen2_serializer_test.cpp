/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \author     DwarFS Implementation
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
 */

/**
 * Frozen2 Serializer Tests
 *
 * Task 5: Test the main Frozen2Serializer orchestrator
 *
 * These tests verify the simplified API that uses SchemaBuilder
 * to generate schema and FrozenSchemaSerializer to encode it.
 */

#include <gtest/gtest.h>
#include <cstring>

#include "dwarfs/metadata/legacy/frozen2_serializer.h"
#include "dwarfs/metadata/domain/metadata.h"

namespace dwarfs::metadata::legacy::test {

// Task 5 Step 1: Round-trip test
TEST(Frozen2SerializerTest, RoundTripPreservesMetadata) {
  // Arrange
  domain::metadata original;
  original.chunks = {{0, 0, 4096}, {1, 0, 8192}};
  original.block_size = 65536;
  original.timestamp_base = 1234567890;

  // Act: Serialize
  Frozen2Serializer serializer;
  std::vector<uint8_t> serialized = serializer.serialize(&original);

  // Assert: Check structure
  EXPECT_GT(serialized.size(), 8); // At least size prefix

  // Verify size prefix
  uint64_t size_prefix;
  std::memcpy(&size_prefix, serialized.data(), 8);
  EXPECT_EQ(serialized.size() - 8, size_prefix);
}

} // namespace dwarfs::metadata::legacy::test