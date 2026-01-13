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

#include <gtest/gtest.h>
#include "dwarfs/metadata/legacy/frozen2_schema_builder.h"
#include "dwarfs/metadata/domain/metadata.h"

using namespace dwarfs::metadata::legacy;
using namespace dwarfs::metadata::domain;

TEST(SchemaBuilderTest, BuildsSchemaFromMetadata) {
  // Arrange
  metadata meta;
  meta.chunks = {{0, 0, 4096}, {1, 0, 4096}};
  meta.block_size = 65536;

  SchemaBuilder builder;

  // Act
  Schema schema = builder.build_from(meta);

  // Assert
  EXPECT_EQ(1, schema.file_version);
  EXPECT_TRUE(schema.relax_type_checks);
  EXPECT_NE(0, schema.root_layout);

  auto* root_layout = schema.layouts.get(schema.root_layout);
  ASSERT_NE(root_layout, nullptr);
  EXPECT_GT(root_layout->fields.size(), 0);
}

TEST(SchemaBuilderTest, GeneratesChunkLayout) {
  metadata meta;
  meta.chunks = {{0, 0, 4096}, {1, 100, 8192}, {2, 200, 16384}};

  SchemaBuilder builder;
  Schema schema = builder.build_from(meta);

  // Verify chunk layout was created
  auto* root_layout = schema.layouts.get(schema.root_layout);
  ASSERT_NE(root_layout, nullptr);

  auto* field1 = root_layout->fields.get(1); // chunks field
  ASSERT_NE(field1, nullptr);

  auto* chunk_layout = schema.layouts.get(field1->layout_id);
  ASSERT_NE(chunk_layout, nullptr);

  // Chunk should have 3 fields (block, offset, size)
  // Note: DenseMap::size() returns vector size, not element count
  // We check for the 3 fields by verifying they exist
  EXPECT_NE(chunk_layout->fields.get(1), nullptr);
  EXPECT_NE(chunk_layout->fields.get(2), nullptr);
  EXPECT_NE(chunk_layout->fields.get(3), nullptr);
}

TEST(SchemaBuilderTest, ChunkLayoutHasCorrectOffsets) {
  metadata meta;
  meta.chunks = {{0, 0, 4096}};

  SchemaBuilder builder;
  Schema schema = builder.build_from(meta);

  auto* root_layout = schema.layouts.get(schema.root_layout);
  auto* field1 = root_layout->fields.get(1);
  auto* chunk_layout = schema.layouts.get(field1->layout_id);

  auto* field1_chunk = chunk_layout->fields.get(1);
  auto* field2_chunk = chunk_layout->fields.get(2);
  auto* field3_chunk = chunk_layout->fields.get(3);

  EXPECT_EQ(0, field1_chunk->offset);
  EXPECT_EQ(32, field2_chunk->offset);
  EXPECT_EQ(64, field3_chunk->offset);
}
