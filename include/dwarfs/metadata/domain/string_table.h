/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \file
 * \brief String table domain model
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

#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include <cereal/cereal.hpp>
#include <cereal/types/optional.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>

namespace dwarfs::metadata::domain {

/**
 * An (optionally packed) string table
 */
struct string_table {
  /// Raw buffer containing the concatenation of all individual,
  /// potentially compressed, strings
  std::string buffer;

  /// Symbol table for fsst compression; if fsst is not used, this
  /// will not be set and `buffer` will contain uncompressed strings
  std::optional<std::string> symtab;

  /// The (optionally packed) index; if packed, the index is stored
  /// delta-compressed
  std::vector<uint32_t> index;

  /// Indicates if the index is packed
  bool packed_index{false};

  /**
   * Cereal serialization support
   */
  template <class Archive>
  void serialize(Archive& ar) {
    ar(CEREAL_NVP(buffer), CEREAL_NVP(symtab),
       CEREAL_NVP(index), CEREAL_NVP(packed_index));
  }
};

} // namespace dwarfs::metadata::domain

CEREAL_CLASS_VERSION(dwarfs::metadata::domain::string_table, 1)