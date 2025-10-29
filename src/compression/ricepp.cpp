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

#include <cereal/archives/binary.hpp>
#include <sstream>

#include <fmt/format.h>

#include <nlohmann/json.hpp>

#include <ricepp/create_decoder.h>
#include <ricepp/create_encoder.h>

#include <dwarfs/compressor_registry.h>
#include <dwarfs/decompressor_registry.h>
#include <dwarfs/error.h>
#include <dwarfs/malloc_byte_buffer.h>
#include <dwarfs/metadata/domain/ricepp_block_header.h>
#include <dwarfs/option_map.h>
#include <dwarfs/varint.h>

#include "base.h"

namespace dwarfs {

namespace {

constexpr int RICEPP_VERSION{1};

class ricepp_block_compressor final : public block_compressor::impl {
 public:
  ricepp_block_compressor(size_t block_size)
      : block_size_{block_size} {}

  ricepp_block_compressor(ricepp_block_compressor const& rhs) = default;

  std::unique_ptr<block_compressor::impl> clone() const override {
    return std::make_unique<ricepp_block_compressor>(*this);
  }

  shared_byte_buffer compress(shared_byte_buffer const& data,
                              std::string const* metadata) const override {
    if (!metadata) {
      DWARFS_THROW(runtime_error,
                   "internal error: ricepp compression requires metadata");
    }

    auto meta = nlohmann::json::parse(*metadata);

    auto endianness = meta["endianness"].get<std::string>();
    auto component_count = meta["component_count"].get<int>();
    auto unused_lsb_count = meta["unused_lsb_count"].get<int>();
    auto bytes_per_sample = meta["bytes_per_sample"].get<int>();

    assert(2 <= bytes_per_sample && bytes_per_sample <= 2);
    assert(0 <= unused_lsb_count && unused_lsb_count <= 8);
    assert(1 <= component_count && component_count <= 2);

    if (data.size() % (component_count * bytes_per_sample)) {
      DWARFS_THROW(runtime_error,
                   fmt::format("unexpected data configuration: {} bytes to "
                               "compress, {} components, {} bytes per sample",
                               data.size(), component_count, bytes_per_sample));
    }

    using pixel_type = uint16_t;
    auto byteorder =
        endianness == "big" ? std::endian::big : std::endian::little;

    auto encoder = ricepp::create_encoder<pixel_type>({
        .block_size = block_size_,
        .component_stream_count = static_cast<size_t>(component_count),
        .byteorder = byteorder,
        .unused_lsb_count = static_cast<unsigned>(unused_lsb_count),
    });

    auto compressed = malloc_byte_buffer::create(); // TODO: make configurable

    // TODO: see if we can resize just once...
    {
      compressed.resize(varint::max_size);

      size_t pos = 0;
      pos += varint::encode(data.size(), compressed.data() + pos);
      compressed.resize(pos);

      dwarfs::metadata::domain::ricepp_block_header hdr;
      hdr.block_size = static_cast<uint32_t>(block_size_);
      hdr.component_count = static_cast<uint16_t>(component_count);
      hdr.bytes_per_sample = static_cast<uint8_t>(bytes_per_sample);
      hdr.unused_lsb_count = static_cast<uint8_t>(unused_lsb_count);
      hdr.big_endian = byteorder == std::endian::big;
      hdr.ricepp_version = RICEPP_VERSION;

      std::ostringstream oss(std::ios::binary);
      {
        cereal::BinaryOutputArchive archive(oss);
        archive(hdr);
      }
      std::string hdrbuf = oss.str();

      compressed.append(hdrbuf.data(), hdrbuf.size());
    }

    std::span<pixel_type const> input{
        reinterpret_cast<pixel_type const*>(data.data()),
        data.size() / bytes_per_sample};

    size_t header_size = compressed.size();
    compressed.resize(header_size + encoder->worst_case_encoded_bytes(input));

    auto output =
        encoder->encode(compressed.span().subspan(header_size), input);
    compressed.resize(header_size + output.size());
    compressed.shrink_to_fit();

    return compressed.share();
  }

  compression_type type() const override { return compression_type::RICEPP; }

  std::string describe() const override {
    return fmt::format("ricepp [block_size={}]", block_size_);
  }

  std::string metadata_requirements() const override {
    using nlj = nlohmann::json;
    nlohmann::json req{
        {"endianness", nlj::array({"set", nlj::array({"big", "little"})})},
        {"bytes_per_sample", nlj::array({"set", nlj::array({2})})},
        {"component_count", nlj::array({"range", 1, 2})},
        {"unused_lsb_count", nlj::array({"range", 0, 8})},
    };
    return req.dump();
  }

  compression_constraints
  get_compression_constraints(std::string const& metadata) const override {
    auto meta = nlohmann::json::parse(metadata);

    auto component_count = meta["component_count"].get<int>();
    auto bytes_per_sample = meta["bytes_per_sample"].get<int>();

    compression_constraints cc;

    cc.granularity = component_count * bytes_per_sample;

    return cc;
  }

  size_t estimate_memory_usage(size_t data_size) const override {
    // ricepp encoder basically has no memory overhead by itself
    return data_size;
  }

 private:
  size_t const block_size_;
};

class ricepp_block_decompressor final : public block_decompressor_base {
 public:
  ricepp_block_decompressor(std::span<uint8_t const> data)
      : uncompressed_size_{varint::decode(data)}
      , header_{decode_header(data)}
      , data_{data}
      , decoder_{ricepp::create_decoder<uint16_t>(
            {.block_size = header_.block_size,
             .component_stream_count = header_.component_count,
             .byteorder = header_.big_endian ? std::endian::big
                                             : std::endian::little,
             .unused_lsb_count = header_.unused_lsb_count})} {
    if (header_.bytes_per_sample != 2) {
      DWARFS_THROW(runtime_error,
                   fmt::format("[RICEPP] unsupported bytes per sample: {}",
                               header_.bytes_per_sample));
    }
  }

  compression_type type() const override { return compression_type::RICEPP; }

  std::optional<std::string> metadata() const override {
    nlohmann::json meta{
        {"endianness", header_.big_endian ? "big" : "little"},
        {"bytes_per_sample", header_.bytes_per_sample},
        {"unused_lsb_count", header_.unused_lsb_count},
        {"component_count", header_.component_count},
    };
    return meta.dump();
  }

  bool decompress_frame(size_t) override {
    DWARFS_CHECK(decompressed_, "decompression not started");

    if (!decoder_) {
      return false;
    }

    decompressed_.resize(uncompressed_size_);
    std::span<uint16_t> output{
        reinterpret_cast<uint16_t*>(decompressed_.data()),
        decompressed_.size() / 2};

    decoder_->decode(output, data_);

    decoder_.reset();

    return true;
  }

  size_t uncompressed_size() const override { return uncompressed_size_; }

 private:
  static dwarfs::metadata::domain::ricepp_block_header
  decode_header(std::span<uint8_t const>& span) {
    dwarfs::metadata::domain::ricepp_block_header hdr;

    std::string str(reinterpret_cast<char const*>(span.data()), span.size());
    std::istringstream iss(str, std::ios::binary);

    std::streampos start_pos = iss.tellg();
    {
      cereal::BinaryInputArchive archive(iss);
      archive(hdr);
    }
    std::streampos end_pos = iss.tellg();

    size_t bytes_read = static_cast<size_t>(end_pos - start_pos);
    span = span.subspan(bytes_read);

    if (hdr.ricepp_version > RICEPP_VERSION) {
      DWARFS_THROW(runtime_error,
                   fmt::format("[RICEPP] unsupported version: {}",
                               hdr.ricepp_version));
    }
    return hdr;
  }

  uint64_t const uncompressed_size_;
  dwarfs::metadata::domain::ricepp_block_header const header_;
  std::span<uint8_t const> data_;
  std::unique_ptr<ricepp::decoder_interface<uint16_t>> decoder_;
};

template <typename Base>
class ricepp_compression_info : public Base {
 public:
  static constexpr compression_type type{compression_type::RICEPP};

  std::string_view name() const override { return "ricepp"; }

  std::string_view description() const override {
    static std::string const s_desc{"RICEPP compression"};
    return s_desc;
  }

  std::set<std::string> library_dependencies() const override { return {}; }
};

class ricepp_compressor_factory final
    : public ricepp_compression_info<compressor_factory> {
 public:
  std::span<std::string const> options() const override { return options_; }

  std::unique_ptr<block_compressor::impl>
  create(option_map& om) const override {
    return std::make_unique<ricepp_block_compressor>(
        om.get<size_t>("block_size", 128));
  }

 private:
  std::vector<std::string> const options_{
      fmt::format("block_size=[{}..{}]", 16, 512),
  };
};

class ricepp_decompressor_factory final
    : public ricepp_compression_info<decompressor_factory> {
 public:
  std::unique_ptr<block_decompressor::impl>
  create(std::span<uint8_t const> data) const override {
    return std::make_unique<ricepp_block_decompressor>(data);
  }
};

} // namespace

REGISTER_COMPRESSOR_FACTORY(ricepp_compressor_factory)
REGISTER_DECOMPRESSOR_FACTORY(ricepp_decompressor_factory)

} // namespace dwarfs
