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

#include "dwarfs/metadata/legacy/frozen_bits.h"

#include <vector>

using namespace dwarfs::metadata::legacy;

class FrozenBitsTest : public ::testing::Test {
protected:
  std::vector<uint8_t> make_buffer(size_t size) {
    return std::vector<uint8_t>(size, 0);
  }
};

// Test loading single bits
TEST_F(FrozenBitsTest, LoadBit_Basic) {
  std::vector<uint8_t> data = {0b10101010, 0b01010101};

  EXPECT_FALSE(frozen_bits::load_bit(data, 0));
  EXPECT_TRUE(frozen_bits::load_bit(data, 1));
  EXPECT_FALSE(frozen_bits::load_bit(data, 2));
  EXPECT_TRUE(frozen_bits::load_bit(data, 3));
  EXPECT_FALSE(frozen_bits::load_bit(data, 4));
  EXPECT_TRUE(frozen_bits::load_bit(data, 5));
  EXPECT_FALSE(frozen_bits::load_bit(data, 6));
  EXPECT_TRUE(frozen_bits::load_bit(data, 7));

  EXPECT_TRUE(frozen_bits::load_bit(data, 8));
  EXPECT_FALSE(frozen_bits::load_bit(data, 9));
  EXPECT_TRUE(frozen_bits::load_bit(data, 10));
  EXPECT_FALSE(frozen_bits::load_bit(data, 11));
}

// Test storing single bits
TEST_F(FrozenBitsTest, StoreBit_Basic) {
  auto data = make_buffer(2);

  frozen_bits::store_bit(data, 0, false);
  frozen_bits::store_bit(data, 1, true);
  frozen_bits::store_bit(data, 2, false);
  frozen_bits::store_bit(data, 3, true);
  frozen_bits::store_bit(data, 7, true);

  EXPECT_EQ(data[0], 0b10001010);

  frozen_bits::store_bit(data, 8, true);
  frozen_bits::store_bit(data, 9, false);
  frozen_bits::store_bit(data, 15, true);

  EXPECT_EQ(data[1], 0b10000001);
}

// Test round-trip for single bits
TEST_F(FrozenBitsTest, BitRoundTrip) {
  auto data = make_buffer(16);

  for (uint32_t i = 0; i < 128; ++i) {
    bool value = (i % 3) == 0; // Arbitrary pattern
    frozen_bits::store_bit(data, i, value);
    EXPECT_EQ(frozen_bits::load_bit(data, i), value) << "Bit " << i;
  }
}

// Test loading bits of various widths
TEST_F(FrozenBitsTest, LoadBits_AllWidths) {
  // Create test data with known pattern
  std::vector<uint8_t> data = {
      0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0, 0x11, 0x22, 0x33, 0x44};

  // Test 8-bit aligned reads
  EXPECT_EQ(frozen_bits::load_bits(data, 0, 8), 0x12);
  EXPECT_EQ(frozen_bits::load_bits(data, 8, 8), 0x34);
  EXPECT_EQ(frozen_bits::load_bits(data, 16, 8), 0x56);

  // Test 16-bit reads
  EXPECT_EQ(frozen_bits::load_bits(data, 0, 16), 0x3412); // Little-endian
  EXPECT_EQ(frozen_bits::load_bits(data, 8, 16), 0x5634);

  // Test 32-bit reads
  EXPECT_EQ(frozen_bits::load_bits(data, 0, 32), 0x78563412);

  // Test unaligned reads
  EXPECT_EQ(frozen_bits::load_bits(data, 4, 8), 0x41); // Bits 4-11: byte0[4-7]=1, byte1[0-3]=4 => 0x41
}

// Test storing bits of various widths
TEST_F(FrozenBitsTest, StoreBits_AllWidths) {
  auto data = make_buffer(16);

  // Test 8-bit aligned writes
  frozen_bits::store_bits(data, 0, 8, 0x12);
  EXPECT_EQ(data[0], 0x12);

  frozen_bits::store_bits(data, 8, 8, 0x34);
  EXPECT_EQ(data[1], 0x34);

  // Test 16-bit writes
  data = make_buffer(16);
  frozen_bits::store_bits(data, 0, 16, 0x3412);
  EXPECT_EQ(data[0], 0x12);
  EXPECT_EQ(data[1], 0x34);

  // Test 32-bit writes
  data = make_buffer(16);
  frozen_bits::store_bits(data, 0, 32, 0x78563412);
  EXPECT_EQ(data[0], 0x12);
  EXPECT_EQ(data[1], 0x34);
  EXPECT_EQ(data[2], 0x56);
  EXPECT_EQ(data[3], 0x78);
}

// Test round-trip for various bit widths
TEST_F(FrozenBitsTest, RoundTrip_VariousWidths) {
  auto data = make_buffer(16);

  // Test all bit widths from 1 to 64
  for (uint16_t bits = 1; bits <= 64; ++bits) {
    data = make_buffer(16);

    // Create a value with the specified number of bits set
    uint64_t mask = (bits == 64) ? UINT64_MAX : ((1ULL << bits) - 1);
    uint64_t value = 0xA5A5A5A5A5A5A5A5ULL & mask;

    frozen_bits::store_bits(data, 0, bits, value);
    uint64_t read = frozen_bits::load_bits(data, 0, bits);

    EXPECT_EQ(read, value) << "Bits: " << bits;
  }
}

// Test full 64-bit values
TEST_F(FrozenBitsTest, RoundTrip_U64_Aligned) {
  auto data = make_buffer(16);

  // Test various 64-bit values
  std::vector<uint64_t> test_values = {
      0x0000000000000000ULL,
      0xFFFFFFFFFFFFFFFFULL,
      0x0123456789ABCDEFULL,
      0xFEDCBA9876543210ULL,
      0x8000000000000000ULL, // MSB set
      0x0000000000000001ULL, // LSB set
      0xAAAAAAAAAAAAAAAAULL,
      0x5555555555555555ULL,
  };

  for (auto value : test_values) {
    data = make_buffer(16);
    frozen_bits::store_bits(data, 0, 64, value);
    uint64_t read = frozen_bits::load_bits(data, 0, 64);
    EXPECT_EQ(read, value) << "Value: 0x" << std::hex << value;
  }
}

// Test 64-bit values at unaligned positions
TEST_F(FrozenBitsTest, RoundTrip_U64_Unaligned) {
  auto data = make_buffer(16);

  uint64_t value = 0x0123456789ABCDEFULL;

  // Test at various bit offsets
  for (uint32_t offset = 0; offset < 8; ++offset) {
    data = make_buffer(16);
    frozen_bits::store_bits(data, offset, 64, value);
    uint64_t read = frozen_bits::load_bits(data, offset, 64);
    EXPECT_EQ(read, value) << "Offset: " << offset;
  }
}

// Test unaligned reads/writes
TEST_F(FrozenBitsTest, RoundTrip_Unaligned) {
  auto data = make_buffer(16);

  // Test 32-bit value at various offsets
  uint64_t value = 0x12345678;

  for (uint32_t offset = 0; offset < 32; ++offset) {
    data = make_buffer(16);
    frozen_bits::store_bits(data, offset, 32, value);
    uint64_t read = frozen_bits::load_bits(data, offset, 32);
    EXPECT_EQ(read, value) << "Offset: " << offset;
  }
}

// Test edge case: reading/writing at byte boundaries
TEST_F(FrozenBitsTest, ByteBoundaries) {
  auto data = make_buffer(16);

  // Write values that span byte boundaries
  frozen_bits::store_bits(data, 4, 16, 0xABCD);
  uint64_t read = frozen_bits::load_bits(data, 4, 16);
  EXPECT_EQ(read, 0xABCD);

  // Verify the actual bytes
  // When writing 0xABCD at bit offset 4:
  // Bits 4-7: 0xD, Bits 8-11: 0xC, Bits 12-15: 0xB, Bits 16-19: 0xA
  EXPECT_EQ(frozen_bits::load_bits(data, 0, 4), 0x0); // Bits 0-3 weren't written
  EXPECT_EQ(frozen_bits::load_bits(data, 4, 12), 0xBCD); // Bits 4-15 contain BCD
}

// Test error handling: invalid bit counts
TEST_F(FrozenBitsTest, ErrorHandling_InvalidBits) {
  auto data = make_buffer(16);

  EXPECT_THROW(frozen_bits::load_bits(data, 0, 0), std::invalid_argument);
  EXPECT_THROW(frozen_bits::load_bits(data, 0, 65), std::invalid_argument);
  EXPECT_THROW(frozen_bits::store_bits(data, 0, 0, 0), std::invalid_argument);
  EXPECT_THROW(frozen_bits::store_bits(data, 0, 65, 0), std::invalid_argument);
}

// Test error handling: out of range
TEST_F(FrozenBitsTest, ErrorHandling_OutOfRange) {
  auto data = make_buffer(8);

  EXPECT_THROW(frozen_bits::load_bit(data, 64), std::out_of_range);
  EXPECT_THROW(frozen_bits::store_bit(data, 64, true), std::out_of_range);
  EXPECT_THROW(frozen_bits::load_bits(data, 64, 8), std::out_of_range);
}

// Test helper functions
TEST_F(FrozenBitsTest, HelperFunctions) {
  EXPECT_EQ(frozen_bits::bytes_for_bits(0), 0);
  EXPECT_EQ(frozen_bits::bytes_for_bits(1), 1);
  EXPECT_EQ(frozen_bits::bytes_for_bits(7), 1);
  EXPECT_EQ(frozen_bits::bytes_for_bits(8), 1);
  EXPECT_EQ(frozen_bits::bytes_for_bits(9), 2);
  EXPECT_EQ(frozen_bits::bytes_for_bits(16), 2);
  EXPECT_EQ(frozen_bits::bytes_for_bits(64), 8);
  EXPECT_EQ(frozen_bits::bytes_for_bits(65), 9);

  EXPECT_EQ(frozen_bits::bytes_for_range(0, 8), 1);
  EXPECT_EQ(frozen_bits::bytes_for_range(4, 8), 2); // Bits 4-11 span 2 bytes
  EXPECT_EQ(frozen_bits::bytes_for_range(7, 2), 2); // Bits 7-8 span 2 bytes
}

// Test complex pattern: multiple values at different offsets
TEST_F(FrozenBitsTest, ComplexPattern) {
  auto data = make_buffer(32);

  // Store multiple values at different offsets and widths
  frozen_bits::store_bits(data, 0, 8, 0x12);
  frozen_bits::store_bits(data, 8, 16, 0x3456);
  frozen_bits::store_bits(data, 24, 32, 0x789ABCDE);
  frozen_bits::store_bits(data, 56, 8, 0xF0);

  // Read them back
  EXPECT_EQ(frozen_bits::load_bits(data, 0, 8), 0x12);
  EXPECT_EQ(frozen_bits::load_bits(data, 8, 16), 0x3456);
  EXPECT_EQ(frozen_bits::load_bits(data, 24, 32), 0x789ABCDE);
  EXPECT_EQ(frozen_bits::load_bits(data, 56, 8), 0xF0);
}

// Test sequential writes don't interfere
TEST_F(FrozenBitsTest, SequentialWrites) {
  auto data = make_buffer(16);

  // Write values in sequence
  frozen_bits::store_bits(data, 0, 4, 0xA);
  frozen_bits::store_bits(data, 4, 4, 0xB);
  frozen_bits::store_bits(data, 8, 4, 0xC);
  frozen_bits::store_bits(data, 12, 4, 0xD);

  // Read them back individually
  EXPECT_EQ(frozen_bits::load_bits(data, 0, 4), 0xA);
  EXPECT_EQ(frozen_bits::load_bits(data, 4, 4), 0xB);
  EXPECT_EQ(frozen_bits::load_bits(data, 8, 4), 0xC);
  EXPECT_EQ(frozen_bits::load_bits(data, 12, 4), 0xD);

  // Read them as combined values
  EXPECT_EQ(frozen_bits::load_bits(data, 0, 8), 0xBA);
  EXPECT_EQ(frozen_bits::load_bits(data, 8, 8), 0xDC);
  EXPECT_EQ(frozen_bits::load_bits(data, 0, 16), 0xDCBA);
}