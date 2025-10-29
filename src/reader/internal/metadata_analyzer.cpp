/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \author     Marcus Holland-Moritz (github@mhxnet.de)
 * \copyright  Copyright (c) Marcus Holland-Moritz
 *
 * This file is part of dwarfs.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the “Software”), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * SPDX-License-Identifier: MIT
 */

#include <cstdint>
#include <ostream>
#include <span>

#include <dwarfs/reader/internal/metadata_analyzer.h>

namespace dwarfs::reader::internal {

/**
 * NOTE: This file is stubbed out during the Thrift/Folly removal.
 * The frozen metadata analysis functionality is not critical for
 * core filesystem operations. It was primarily used for debugging
 * and analyzing metadata layout in the frozen format.
 *
 * This can be re-implemented later using domain models if needed.
 */

metadata_analyzer::metadata_analyzer(std::span<uint8_t const> data)
    : data_{data} {}

void metadata_analyzer::print_layout(std::ostream& os) const {
  os << "metadata layout analysis not available (frozen format deprecated)\n";
}

void metadata_analyzer::print_frozen(std::ostream& os, bool /* verbose */) const {
  os << "frozen metadata analysis not available (frozen format deprecated)\n";
  os << "total data size: " << data_.size() << " bytes\n";
}

} // namespace dwarfs::reader::internal
