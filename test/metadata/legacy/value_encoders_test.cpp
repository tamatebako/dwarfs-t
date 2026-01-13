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

#include "dwarfs/metadata/legacy/value_encoders.h"
#include "dwarfs/metadata/legacy/frozen_schema.h"
#include "dwarfs/metadata/legacy/frozen_bits.h"

#include <vector>
#include <stdexcept>

using namespace dwarfs::metadata::legacy;

// === Basic encoding tests (original tests) ===

TEST(ValueEncoderTest, ScalarEncoder_EncodesU32) {
  // Arrange
  std::vector<uint8_t> buffer(16, 0);
  FrozenWriter writer(buffer);

  SchemaLayout layout;
  layout.bits = 32; // 32-bit unsigned integer

  uint32_t value = 0xDEADBEEF;

  // Act
  ScalarEncoder encoder;
  uint32_t bits_written = encoder.encode(writer, layout, &value);

  // Assert
  EXPECT_EQ(32, bits_written);

  // Verify the value was written correctly
  uint32_t read_back = frozen_bits::load_bits(buffer, 0, 32);
  EXPECT_EQ(value, read_back);
}

TEST(ValueEncoderTest, ScalarEncoder_EncodesU16) {
  std::vector<uint8_t> buffer(8, 0);
  FrozenWriter writer(buffer);

  SchemaLayout layout;
  layout.bits = 16;

  uint16_t value = 0xBEEF;

  ScalarEncoder encoder;
  encoder.encode(writer, layout, &value);

  uint16_t read_back = frozen_bits::load_bits(buffer, 0, 16);
  EXPECT_EQ(value, read_back);
}

TEST(ValueEncoderTest, ScalarEncoder_EncodesU8) {
  std::vector<uint8_t> buffer(8, 0);
  FrozenWriter writer(buffer);

  SchemaLayout layout;
  layout.bits = 8;

  uint8_t value = 0xEF;

  ScalarEncoder encoder;
  encoder.encode(writer, layout, &value);

  uint8_t read_back = static_cast<uint8_t>(frozen_bits::load_bits(buffer, 0, 8));
  EXPECT_EQ(value, read_back);
}

TEST(ValueEncoderTest, ScalarEncoder_EncodesBool) {
  std::vector<uint8_t> buffer(8, 0);
  FrozenWriter writer(buffer);

  SchemaLayout layout;
  layout.bits = 1;

  bool value = true;

  ScalarEncoder encoder;
  encoder.encode(writer, layout, &value);

  bool read_back = frozen_bits::load_bit(buffer, 0);
  EXPECT_EQ(value, read_back);
}

TEST(ValueEncoderTest, ScalarEncoder_EncodesBoolFalse) {
  std::vector<uint8_t> buffer(8, 0);
  FrozenWriter writer(buffer);

  SchemaLayout layout;
  layout.bits = 1;

  bool value = false;

  ScalarEncoder encoder;
  encoder.encode(writer, layout, &value);

  bool read_back = frozen_bits::load_bit(buffer, 0);
  EXPECT_EQ(value, read_back);
}

// === Edge case tests (new tests) ===

TEST(ValueEncoderTest, ScalarEncoder_EncodesU64) {
  std::vector<uint8_t> buffer(16, 0);
  FrozenWriter writer(buffer);

  SchemaLayout layout;
  layout.bits = 64;

  uint64_t value = 0xFFFFFFFFFFFFFFFFULL;

  ScalarEncoder encoder;
  encoder.encode(writer, layout, &value);

  uint64_t read_back = frozen_bits::load_bits(buffer, 0, 64);
  EXPECT_EQ(value, read_back);
}

TEST(ValueEncoderTest, ScalarEncoder_EncodesU32AllBitsSet) {
  std::vector<uint8_t> buffer(8, 0);
  FrozenWriter writer(buffer);

  SchemaLayout layout;
  layout.bits = 32;

  uint32_t value = 0xFFFFFFFF;

  ScalarEncoder encoder;
  encoder.encode(writer, layout, &value);

  uint32_t read_back = frozen_bits::load_bits(buffer, 0, 32);
  EXPECT_EQ(value, read_back);
}

TEST(ValueEncoderTest, ScalarEncoder_EncodesU16AllBitsSet) {
  std::vector<uint8_t> buffer(8, 0);
  FrozenWriter writer(buffer);

  SchemaLayout layout;
  layout.bits = 16;

  uint16_t value = 0xFFFF;

  ScalarEncoder encoder;
  encoder.encode(writer, layout, &value);

  uint16_t read_back = frozen_bits::load_bits(buffer, 0, 16);
  EXPECT_EQ(value, read_back);
}

TEST(ValueEncoderTest, ScalarEncoder_EncodesU8AllBitsSet) {
  std::vector<uint8_t> buffer(8, 0);
  FrozenWriter writer(buffer);

  SchemaLayout layout;
  layout.bits = 8;

  uint8_t value = 0xFF;

  ScalarEncoder encoder;
  encoder.encode(writer, layout, &value);

  uint8_t read_back = static_cast<uint8_t>(frozen_bits::load_bits(buffer, 0, 8));
  EXPECT_EQ(value, read_back);
}

TEST(ValueEncoderTest, ScalarEncoder_EncodesAtBitOffset) {
  std::vector<uint8_t> buffer(16, 0);
  FrozenWriter writer(buffer);

  // Write 4 bits first to create offset
  writer.write_bits(0xA, 4);

  SchemaLayout layout;
  layout.bits = 16;

  uint16_t value = 0xBEEF;

  ScalarEncoder encoder;
  encoder.encode(writer, layout, &value);

  // Read back at offset 4
  uint16_t read_back = frozen_bits::load_bits(buffer, 4, 16);
  EXPECT_EQ(value, read_back);
}

TEST(ValueEncoderTest, ScalarEncoder_EncodesU32MultipleValues) {
  std::vector<uint8_t> buffer(16, 0);
  FrozenWriter writer(buffer);

  SchemaLayout layout;
  layout.bits = 32;

  ScalarEncoder encoder;

  uint32_t v1 = 0xDEADBEEF;
  uint32_t v2 = 0xCAFEBABE;
  uint32_t v3 = 0xFEEDFACE;

  encoder.encode(writer, layout, &v1);
  encoder.encode(writer, layout, &v2);
  encoder.encode(writer, layout, &v3);

  uint32_t r1 = frozen_bits::load_bits(buffer, 0, 32);
  uint32_t r2 = frozen_bits::load_bits(buffer, 32, 32);
  uint32_t r3 = frozen_bits::load_bits(buffer, 64, 32);

  EXPECT_EQ(v1, r1);
  EXPECT_EQ(v2, r2);
  EXPECT_EQ(v3, r3);
}

TEST(ValueEncoderTest, ScalarEncoder_EncodesBoundaryValues) {
  std::vector<uint8_t> buffer(16, 0);
  FrozenWriter writer(buffer);

  SchemaLayout layout;
  layout.bits = 32;

  ScalarEncoder encoder;

  // Test minimum value
  uint32_t min_val = 0;
  encoder.encode(writer, layout, &min_val);
  uint32_t min_read = frozen_bits::load_bits(buffer, 0, 32);
  EXPECT_EQ(min_val, min_read);

  // Reset buffer
  std::fill(buffer.begin(), buffer.end(), 0);
  writer = FrozenWriter(buffer);

  // Test maximum value
  uint32_t max_val = 0xFFFFFFFF;
  encoder.encode(writer, layout, &max_val);
  uint32_t max_read = frozen_bits::load_bits(buffer, 0, 32);
  EXPECT_EQ(max_val, max_read);
}

TEST(ValueEncoderTest, ScalarEncoder_EncodesOddBitWidths) {
  std::vector<uint8_t> buffer(16, 0);
  FrozenWriter writer(buffer);

  // Test 7-bit value
  SchemaLayout layout7;
  layout7.bits = 7;
  uint8_t val7 = 0x7F; // 7 bits set

  ScalarEncoder encoder;
  encoder.encode(writer, layout7, &val7);
  uint8_t read7 = static_cast<uint8_t>(frozen_bits::load_bits(buffer, 0, 7));
  EXPECT_EQ(val7, read7);

  // Test 13-bit value
  writer = FrozenWriter(buffer);
  SchemaLayout layout13;
  layout13.bits = 13;
  uint16_t val13 = 0x1FFF; // 13 bits set

  encoder.encode(writer, layout13, &val13);
  uint16_t read13 = static_cast<uint16_t>(frozen_bits::load_bits(buffer, 0, 13));
  EXPECT_EQ(val13, read13);
}

// === Input validation tests ===

TEST(ValueEncoderTest, ScalarEncoder_ThrowsOnNullValue) {
  std::vector<uint8_t> buffer(16, 0);
  FrozenWriter writer(buffer);

  SchemaLayout layout;
  layout.bits = 32;

  ScalarEncoder encoder;

  EXPECT_THROW(
    encoder.encode(writer, layout, nullptr),
    std::invalid_argument
  );
}

TEST(ValueEncoderTest, FrozenWriter_ThrowsOnZeroBits) {
  std::vector<uint8_t> buffer(16, 0);
  FrozenWriter writer(buffer);

  EXPECT_THROW(
    writer.write_bits(0xFF, 0),
    std::invalid_argument
  );
}

TEST(ValueEncoderTest, FrozenWriter_ThrowsOnBitsGreaterThan64) {
  std::vector<uint8_t> buffer(16, 0);
  FrozenWriter writer(buffer);

  EXPECT_THROW(
    writer.write_bits(0xFF, 65),
    std::invalid_argument
  );
}

TEST(ValueEncoderTest, FrozenWriter_ThrowsOnBufferOverflow) {
  // Tiny buffer that will overflow
  std::vector<uint8_t> buffer(1, 0);
  FrozenWriter writer(buffer);

  SchemaLayout layout;
  layout.bits = 32;

  uint32_t value = 0xDEADBEEF;

  ScalarEncoder encoder;

  EXPECT_THROW(
    encoder.encode(writer, layout, &value),
    std::runtime_error
  );
}
