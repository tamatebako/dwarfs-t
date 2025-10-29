/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \author     Marcus Holland-Moritz (github@mhxnet.de)
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
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <dwarfs/logger.h>
#include <dwarfs/malloc_byte_buffer.h>

#include <dwarfs/writer/internal/metadata_freezer.h>

#include <dwarfs/metadata/domain/metadata.h>
#include <dwarfs/metadata/serialization/cereal_binary_serializer.h>

namespace dwarfs::writer::internal {

namespace {

std::pair<shared_byte_buffer, shared_byte_buffer>
freeze_to_buffer(metadata::domain::metadata const& domain_meta) {
  using namespace ::dwarfs::metadata::serialization;

  // Serialize using Cereal
  CerealBinarySerializer serializer;
  auto serialized_data = serializer.serialize(domain_meta);

  // Create buffer from serialized data
  auto data_buffer = malloc_byte_buffer::create(
      std::string(serialized_data.begin(), serialized_data.end()));

  // Return empty schema buffer (not used with Cereal) and data buffer
  auto schema_buffer = malloc_byte_buffer::create(std::string{});

  return {schema_buffer.share(), data_buffer.share()};
}

template <typename LoggerPolicy>
class metadata_freezer_ : public metadata_freezer::impl {
 public:
  explicit metadata_freezer_(logger& lgr)
      : LOG_PROXY_INIT(lgr) {}

  std::pair<shared_byte_buffer, shared_byte_buffer>
  freeze(metadata::domain::metadata const& data) const override {
    auto ti = LOG_TIMED_VERBOSE;
    auto rv = freeze_to_buffer(data);

    ti << "freezing metadata to " << rv.second.size() << " bytes...";

    return rv;
  }

 private:
  LOG_PROXY_DECL(LoggerPolicy);
};

} // namespace

metadata_freezer::metadata_freezer(logger& lgr)
    : impl_{
          make_unique_logging_object<impl, metadata_freezer_, logger_policies>(
              lgr)} {}

metadata_freezer::~metadata_freezer() = default;

} // namespace dwarfs::writer::internal
