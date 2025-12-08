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

#include <fmt/format.h>

#include <nlohmann/json.hpp>

#include <ricepp/create_decoder.h>
#include <ricepp/create_encoder.h>

#ifdef DWARFS_HAVE_THRIFT
#include <thrift/lib/cpp2/protocol/Serializer.h>
#include <dwarfs/gen-cpp2/compression_types.h>
#endif

#include <dwarfs/compressor_registry.h>
#include <dwarfs/decompressor_registry.h>
#include <dwarfs/error.h>
#include <dwarfs/malloc_byte_buffer.h>
#include <dwarfs/option_map.h>
#include <dwarfs/varint.h>

#include "base.h"

namespace dwarfs {

namespace {

constexpr int RICEPP_VERSION{1};

// Format-agnostic Rice++ block header (works with or without Thrift)
struct ricepp_block_header {
  uint32_t block_size;
  uint16_t component_count;
  uint8_t bytes_per_sample;
  uint8_t unused_lsb_count;
  bool big_endian;
  uint16_t ricepp_version;

  // Serialize to binary format (11 bytes total)
  void serialize_to(std::vector<uint8_t>& buf) const {
    buf.push_back(static_cast<uint8_t>(block_size & 0xFF));
    buf.push_back(static_cast<uint8_t>((block_size >> 8) & 0xFF));
    buf.push_back(static_cast<uint8_t>((block_size >> 16) & 0xFF));
    buf.push_back(static_cast<uint8_t>((block_size >> 24) & 0xFF));
    buf.push_back(static_cast<uint8_t>(component_count & 0xFF));
    buf.push_back(static_cast<uint8_t>((component_count >> 8) & 0xFF));
    buf.push_back(bytes_per_sample);
    buf.push_back(unused_lsb_count);
    buf.push_back(big_endian ? 1 : 0);
    buf.push_back(static_cast<uint8_t>(ricepp_version & 0xFF));
    buf.push_back(static_cast<uint8_t>((ricepp_version >> 8) & 0xFF));
  }

  // Deserialize from binary format
  static ricepp_block_header deserialize_from(std::span<uint8_t const>& data) {
    if (data.size() < 11) {
      DWARFS_THROW(runtime_error, "Rice++ header too small");
    }
    ricepp_block_header hdr;
    hdr.block_size = static_cast<uint32_t>(data[0]) |
                    (static_cast<uint32_t>(data[1]) << 8) |
                    (static_cast<uint32_t>(data[2]) << 16) |
                    (static_cast<uint32_t>(data[3]) << 24);
    hdr.component_count = static_cast<uint16_t>(data[4]) |
                         (static_cast<uint16_t>(data[5]) << 8);
    hdr.bytes_per_sample = data[6];
    hdr.unused_lsb_count = data[7];
    hdr.big_endian = data[8] != 0;
    hdr.ricepp_version = static_cast<uint16_t>(data[9]) |
                        (static_cast<uint16_t>(data[10]) << 8);
    data = data.subspan(11);
    return hdr;
  }

#ifdef DWARFS_HAVE_THRIFT
  // Convert to/from Thrift format for backward compatibility
  static ricepp_block_header from_thrift(
      thrift::compression::ricepp_block_header const& thrift_hdr) {
    return {
      thrift_hdr.block_size().value(),
      static_cast<uint16_t>(thrift_hdr.component_count().value()),
      static_cast<uint8_t>(thrift_hdr.bytes_per_sample().value()),
      static_cast<uint8_t>(thrift_hdr.unused_lsb_count().value()),
      thrift_hdr.big_endian().value(),
      static_cast<uint16_t>(thrift_hdr.ricepp_version().value())
    };
  }

  thrift::compression::ricepp_block_header to_thrift() const {
    thrift::compression::ricepp_block_header thrift_hdr;
    thrift_hdr.block_size() = block_size;
    thrift_hdr.component_count() = component_count;
    thrift_hdr.bytes_per_sample() = bytes_per_sample;
    thrift_hdr.unused_lsb_count() = unused_lsb_count;
    thrift_hdr.big_endian() = big_endian;
    thrift_hdr.ricepp_version() = ricepp_version;
    return thrift_hdr;
  }
#endif
};

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

    compressed.resize(varint::max_size);

    size_t pos = 0;
    pos += varint::encode(data.size(), compressed.data() + pos);
    compressed.resize(pos);

    ricepp_block_header hdr;
    hdr.block_size = block_size_;
    hdr.component_count = component_count;
    hdr.bytes_per_sample = bytes_per_sample;
    hdr.unused_lsb_count = unused_lsb_count;
    hdr.big_endian = byteorder == std::endian::big;
    hdr.ricepp_version = RICEPP_VERSION;

    std::vector<uint8_t> hdrbuf;
    hdr.serialize_to(hdrbuf);
    compressed.append(hdrbuf.data(), hdrbuf.size());

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
  static ricepp_block_header
  decode_header(std::span<uint8_t const>& span) {
    auto hdr = ricepp_block_header::deserialize_from(span);
    if (hdr.ricepp_version > RICEPP_VERSION) {
      DWARFS_THROW(runtime_error,
                   fmt::format("[RICEPP] unsupported version: {}",
                               hdr.ricepp_version));
    }
    return hdr;
  }

  uint64_t const uncompressed_size_;
  ricepp_block_header const header_;
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
