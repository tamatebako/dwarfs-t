/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \author     Marcus Holland-Moritz (github@mhx.github.io)
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
#include "dwarfs/metadata/legacy/frozen_writer.h"
#include "dwarfs/metadata/legacy/frozen_bits.h"
#include <vector>

using namespace dwarfs::metadata::legacy;

TEST(FrozenWriterTest, WritesScalarValue) {
  // Arrange
  std::vector<uint8_t> buffer(32);
  FrozenWriter writer(buffer);

  uint32_t value = 0x12345678;

  // Act
  writer.write_scalar(value, 32);

  // Assert
  EXPECT_EQ(32, writer.current_bit_offset());

  uint32_t read_back = frozen_bits::load_bits(buffer, 0, 32);
  EXPECT_EQ(value, read_back);
}

TEST(FrozenWriterTest, ReservesStorageSpace) {
  std::vector<uint8_t> buffer(32);
  FrozenWriter writer(buffer);

  uint32_t offset = writer.reserve_storage(16);

  EXPECT_EQ(0, offset);
  EXPECT_EQ(16, writer.storage_size());
}

TEST(FrozenWriterTest, FinalizeCombinesSections) {
  std::vector<uint8_t> buffer(64);
  FrozenWriter writer(buffer);

  // Write some scalar data
  writer.write_scalar(0xDEADBEEF, 32);

  // Reserve and write storage
  uint32_t storage_offset = writer.reserve_storage(4);
  std::vector<uint8_t> storage_data = {1, 2, 3, 4};
  writer.write_storage(storage_offset, storage_data);

  // Finalize to combine sections
  writer.finalize();

  // Verify compact data was written
  uint32_t read_back = frozen_bits::load_bits(buffer, 0, 32);
  EXPECT_EQ(0xDEADBEEF, read_back);

  // Verify storage was appended after compact data
  size_t data_bytes = (writer.current_bit_offset() + 7) / 8;
  EXPECT_EQ(1, buffer[data_bytes + storage_offset]);
  EXPECT_EQ(2, buffer[data_bytes + storage_offset + 1]);
  EXPECT_EQ(3, buffer[data_bytes + storage_offset + 2]);
  EXPECT_EQ(4, buffer[data_bytes + storage_offset + 3]);
}

TEST(FrozenWriterTest, ThrowsForInvalidBitWidth) {
  std::vector<uint8_t> buffer(32);
  FrozenWriter writer(buffer);

  EXPECT_THROW(writer.write_scalar(0, 0), std::invalid_argument);
  EXPECT_THROW(writer.write_scalar(0, 65), std::invalid_argument);
}

TEST(FrozenWriterTest, WritesAtByteBoundary) {
  std::vector<uint8_t> buffer(32);
  FrozenWriter writer(buffer);

  writer.write_scalar(0xFF, 8);
  writer.write_scalar(0xAA, 8);

  EXPECT_EQ(16, writer.current_bit_offset());
  EXPECT_EQ(0xFF, frozen_bits::load_bits(buffer, 0, 8));
  EXPECT_EQ(0xAA, frozen_bits::load_bits(buffer, 8, 8));
}

TEST(FrozenWriterTest, ThrowsForSmallBuffer) {
  std::vector<uint8_t> buffer(15);
  EXPECT_THROW(FrozenWriter writer(buffer), std::invalid_argument);
}

TEST(FrozenWriterTest, WritesOneBit) {
  std::vector<uint8_t> buffer(32);
  FrozenWriter writer(buffer);

  writer.write_scalar(1, 1);

  EXPECT_EQ(1, writer.current_bit_offset());
  bool value = frozen_bits::load_bit(buffer, 0);
  EXPECT_TRUE(value);
}

TEST(FrozenWriterTest, Writes64Bits) {
  std::vector<uint8_t> buffer(32);
  FrozenWriter writer(buffer);

  uint64_t value = 0xFFFFFFFFFFFFFFFFULL;
  writer.write_scalar(value, 64);

  EXPECT_EQ(64, writer.current_bit_offset());
  uint64_t read_back = frozen_bits::load_bits(buffer, 0, 64);
  EXPECT_EQ(value, read_back);
}

TEST(FrozenWriterTest, MultipleSequentialWrites) {
  std::vector<uint8_t> buffer(32);
  FrozenWriter writer(buffer);

  writer.write_scalar(0x12, 8);
  writer.write_scalar(0x34, 8);
  writer.write_scalar(0x56, 8);

  EXPECT_EQ(24, writer.current_bit_offset());
  EXPECT_EQ(0x12, frozen_bits::load_bits(buffer, 0, 8));
  EXPECT_EQ(0x34, frozen_bits::load_bits(buffer, 8, 8));
  EXPECT_EQ(0x56, frozen_bits::load_bits(buffer, 16, 8));
}

TEST(FrozenWriterTest, ZeroStorageReservation) {
  std::vector<uint8_t> buffer(32);
  FrozenWriter writer(buffer);

  uint32_t offset = writer.reserve_storage(0);

  EXPECT_EQ(0, offset);
  EXPECT_EQ(0, writer.storage_size());
}
