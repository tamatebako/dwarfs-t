/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \author     Marcus Holland-Moritz (github@mhxnet.de)
 * \author     Ribose Inc.
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
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace dwarfs::metadata::legacy {

/**
 * FSST (Fast Static Symbol Table) encoder for compressing string tables.
 *
 * This is a thin wrapper around the libfsst library to provide bulk
 * compression of string data for the packed_names format.
 */
class fsst_encoder {
 public:
  struct bulk_compression_result {
    std::string dictionary;
    std::string buffer;
    std::vector<std::string_view> compressed_data;
  };

  /**
   * Compress a span of string views using FSST.
   *
   * @param data Input strings to compress
   * @param force Force compression even if it doesn't reduce size
   * @return Compression result if compression was beneficial, nullopt otherwise
   */
  static std::optional<bulk_compression_result>
  compress(std::span<std::string_view const> data, bool force = false);

  /**
   * Compress a span of strings using FSST.
   *
   * @param data Input strings to compress
   * @param force Force compression even if it doesn't reduce size
   * @return Compression result if compression was beneficial, nullopt otherwise
   */
  static std::optional<bulk_compression_result>
  compress(std::span<std::string const> data, bool force = false);
};

/**
 * FSST (Fast Static Symbol Table) decoder for decompressing string tables.
 *
 * This class wraps the libfsst decoder and provides decompression of
 * FSST-compressed string data using a symbol table dictionary.
 */
class fsst_decoder {
 public:
  /**
   * Create a decoder from an FSST symbol table dictionary.
   *
   * @param dictionary The FSST symbol table (128 bytes typically)
   */
  explicit fsst_decoder(std::string_view dictionary);

  /**
   * Decompress FSST-compressed data.
   *
   * @param data Compressed input data
   * @return Decompressed string
   */
  std::string decompress(std::string_view data) const {
    return impl_->decompress(data);
  }

  class impl {
   public:
    virtual ~impl() = default;

    virtual std::string decompress(std::string_view data) const = 0;
  };

 private:
  std::unique_ptr<impl const> impl_;
};

} // namespace dwarfs::metadata::legacy
