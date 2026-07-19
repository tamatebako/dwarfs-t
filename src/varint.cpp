/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \author     Marcus Holland-Moritz (github@mhxnet.de)
 * \copyright  Copyright (c) Marcus Holland-Moritz
 *
 * This file is part of dwarfs.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * SPDX-License-Identifier: MIT
 */

#include <dwarfs/internal/folly_compat.h>
#include <dwarfs/varint.h>

namespace dwarfs {

// Free function implementations (legacy)
size_t size_of_varint(uint64_t val) {
  uint8_t buf[10];
  return compat::encodeVarint(val, buf);
}

size_t encode_varint(uint64_t val, uint8_t* buf) {
  return compat::encodeVarint(val, buf);
}

size_t decode_varint(uint8_t const* buf, uint64_t* val) {
  return compat::decodeVarint(buf, val);
}

// Class method implementations
size_t varint::encode(value_type value, uint8_t* buffer) {
  return compat::encodeVarint(value, buffer);
}

varint::value_type varint::decode(std::span<uint8_t const>& buffer) {
  value_type result = 0;
  size_t const bytes_read = compat::decodeVarint(buffer.data(), &result);
  buffer = buffer.subspan(bytes_read);
  return result;
}

} // namespace dwarfs
