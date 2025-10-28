/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \file
 * \brief Auto-registration helper for serializers
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

#pragma once

#include <memory>
#include <vector>

#include "serializer_registry.h"

namespace dwarfs::metadata::serialization {

/**
 * Auto-registration helper for serializers
 *
 * Template class that automatically registers a serializer with the
 * registry during static initialization. This enables the plugin
 * architecture where serializers self-register.
 *
 * Design Principles:
 * - **Static Initialization**: Registers at program startup
 * - **Template Metaprogramming**: Type-safe registration
 * - **Zero Runtime Cost**: Registration happens before main()
 * - **Declarative**: Simple one-line registration
 *
 * \tparam SerializerT The serializer class to register
 *
 * \example
 * \code
 * // In cereal_binary_serializer.cpp:
 * namespace {
 *   SerializerAutoRegister<CerealBinarySerializer> cereal_registration(
 *       "cereal_binary",
 *       {0xCE, 0xEA, 0x01},
 *       100,
 *       SerializationFormat::CEREAL_BINARY
 *   );
 * }
 * \endcode
 */
template <typename SerializerT>
class SerializerAutoRegister {
public:
  /**
   * Constructor that performs registration
   *
   * Automatically called during static initialization before main().
   * Registers the serializer with the global registry.
   *
   * \param name Format identifier (e.g., "cereal_binary")
   * \param magic_bytes Magic bytes for format detection
   * \param priority Detection priority (higher = checked first)
   * \param format Format enum value
   *
   * \example
   * \code
   * static SerializerAutoRegister<CerealBinarySerializer> registration(
   *     "cereal_binary",
   *     {0xCE, 0xEA, 0x01},
   *     100,
   *     SerializationFormat::CEREAL_BINARY
   * );
   * \endcode
   */
  SerializerAutoRegister(const std::string& name,
                         const std::vector<uint8_t>& magic_bytes, int priority,
                         SerializationFormat format) {
    SerializerRegistry::instance().register_serializer(
        name,
        []() -> std::unique_ptr<IMetadataSerializer> {
          return std::make_unique<SerializerT>();
        },
        magic_bytes, priority, format);
  }

  // Delete copy and move constructors/operators
  SerializerAutoRegister(const SerializerAutoRegister&) = delete;
  SerializerAutoRegister& operator=(const SerializerAutoRegister&) = delete;
  SerializerAutoRegister(SerializerAutoRegister&&) = delete;
  SerializerAutoRegister& operator=(SerializerAutoRegister&&) = delete;
};

/**
 * Convenience macro for serializer registration
 *
 * Simplifies the registration syntax by hiding the template instantiation
 * and anonymous namespace boilerplate.
 *
 * \param SerializerClass The serializer class to register
 * \param name Format identifier string
 * \param magic_bytes Initializer list of magic bytes
 * \param priority Detection priority integer
 * \param format SerializationFormat enum value
 *
 * \example
 * \code
 * // In cereal_binary_serializer.cpp:
 * REGISTER_SERIALIZER(CerealBinarySerializer,
 *                     "cereal_binary",
 *                     {0xCE, 0xEA, 0x01},
 *                     100,
 *                     SerializationFormat::CEREAL_BINARY);
 * \endcode
 */
#define REGISTER_SERIALIZER(SerializerClass, name, magic_bytes, priority, \
                            format)                                        \
  namespace {                                                              \
  static ::dwarfs::metadata::serialization::SerializerAutoRegister<        \
      SerializerClass>                                                     \
      serializer_registration_##SerializerClass(name, magic_bytes,         \
                                                 priority, format);        \
  }

} // namespace dwarfs::metadata::serialization