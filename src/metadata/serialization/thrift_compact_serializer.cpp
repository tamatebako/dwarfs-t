/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \file
 * \brief Thrift compact serializer auto-registration
 * \author Marcus Holland-Moritz (github@mhxnet.de)
 * \copyright Copyright (c) Marcus Holland-Moritz
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

#include "dwarfs/metadata/serialization/auto_register.h"

#ifdef DWARFS_LEGACY_THRIFT_SUPPORT
#include "dwarfs/metadata/serialization/thrift_compact_serializer.h"

namespace dwarfs::metadata::serialization {

// Auto-register the Thrift Compact serializer at static initialization time
// Only when legacy Thrift support is enabled
namespace {
SerializerAutoRegister<ThriftCompactSerializer> thrift_compact_registration(
    "thrift_compact",
    {0x82, 0x21}, // Magic bytes from config
    50,           // Priority from config
    SerializationFormat::THRIFT_COMPACT);
} // namespace

} // namespace dwarfs::metadata::serialization

#endif // DWARFS_LEGACY_THRIFT_SUPPORT