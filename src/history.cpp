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

#include <ostream>
#include <sstream>
#include <set>
#include <ctime>

#include <cereal/archives/binary.hpp>
#include <cereal/types/optional.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/set.hpp>

#include <fmt/chrono.h>
#include <fmt/format.h>

#include <dwarfs/config.h>
#include <dwarfs/history.h>
#include <dwarfs/library_dependencies.h>
#include <dwarfs/malloc_byte_buffer.h>
#include <dwarfs/util.h>
#include <dwarfs/version.h>

namespace dwarfs {

namespace {

struct version_info {
  uint32_t major{0};
  uint32_t minor{0};
  uint32_t patch{0};
  bool is_release{false};
  std::string git_rev;
  std::string git_branch;
  std::string git_desc;

  template <class Archive>
  void serialize(Archive& ar) {
    ar(major, minor, patch, is_release, git_rev, git_branch, git_desc);
  }
};

struct history_entry {
  version_info version;
  std::string system_id;
  std::string compiler_id;
  std::optional<std::vector<std::string>> arguments;
  std::optional<std::time_t> timestamp;
  std::optional<std::set<std::string>> library_versions;

  template <class Archive>
  void serialize(Archive& ar) {
    ar(version, system_id, compiler_id, arguments, timestamp, library_versions);
  }
};

} // namespace

// Define history_data in dwarfs namespace (not anonymous) to match forward declaration
struct history_data {
  std::vector<history_entry> entries;

  template <class Archive>
  void serialize(Archive& ar) {
    ar(entries);
  }
};

history::history(history_config const& cfg)
    : history_{std::make_unique<history_data>()}
    , cfg_{cfg} {}

history::history(history&&) noexcept = default;
history::~history() noexcept = default;  // Must be defined after history_data is complete
history& history::operator=(history&&) noexcept = default;

void history::parse(std::span<uint8_t const> data) {
  history_->entries.clear();
  parse_append(data);
}

void history::parse_append(std::span<uint8_t const> data) {
  std::string buffer(reinterpret_cast<char const*>(data.data()), data.size());
  std::istringstream iss(buffer, std::ios::binary);

  history_data tmp;
  {
    cereal::BinaryInputArchive ar(iss);
    ar(tmp);
  }

  if (!cfg_.with_timestamps) {
    for (auto& entry : tmp.entries) {
      entry.timestamp.reset();
    }
  }

  history_->entries.insert(history_->entries.end(),
                          std::make_move_iterator(tmp.entries.begin()),
                          std::make_move_iterator(tmp.entries.end()));
}

void history::append(
    std::optional<std::vector<std::string>> args,
    std::function<void(library_dependencies&)> const& extra_deps) {
  auto& histent = history_->entries.emplace_back();
  auto& version = histent.version;
  version.major = DWARFS_VERSION_MAJOR;
  version.minor = DWARFS_VERSION_MINOR;
  version.patch = DWARFS_VERSION_PATCH;
  version.is_release = std::string_view(DWARFS_GIT_DESC) == DWARFS_GIT_ID;
  version.git_rev = DWARFS_GIT_REV;
  version.git_branch = DWARFS_GIT_BRANCH;
  version.git_desc = DWARFS_GIT_DESC;
  histent.system_id = DWARFS_SYSTEM_ID;
  histent.compiler_id = DWARFS_COMPILER_ID;
  if (args) {
    histent.arguments = std::move(*args);
  }
  if (cfg_.with_timestamps) {
    histent.timestamp = std::time(nullptr);
  }
  library_dependencies deps;
  deps.add_common_libraries();
  if (extra_deps) {
    extra_deps(deps);
  }
  histent.library_versions = deps.as_set();
}

size_t history::size() const { return history_->entries.size(); }

shared_byte_buffer history::serialize() const {
  std::ostringstream oss(std::ios::binary);
  {
    cereal::BinaryOutputArchive ar(oss);
    ar(*history_);
  }
  std::string buf = oss.str();
  return malloc_byte_buffer::create(buf).share();
}

void history::dump(std::ostream& os) const {
  if (!history_->entries.empty()) {
    size_t const iwidth{std::to_string(history_->entries.size()).size()};
    size_t i{1};

    os << "History:\n";
    for (auto const& histent : history_->entries) {
      os << "  " << fmt::format("{:>{}}:", i++, iwidth);

      if (histent.timestamp.has_value()) {
        os << " [" << fmt::format("{:%F %T}", safe_localtime(histent.timestamp.value()))
           << "]";
      }

      auto const& version = histent.version;

      os << " libdwarfs " << version.git_desc;

      if (!version.is_release) {
        os << " (" << version.git_branch << ")";
      }

      os << " on " << histent.system_id << ", "
         << histent.compiler_id << "\n";

      if (histent.arguments.has_value() && !histent.arguments->empty()) {
        os << fmt::format("  {:>{}}  args:", "", iwidth);
        for (auto const& arg : histent.arguments.value()) {
          os << ' ' << arg;
        }
        os << "\n";
      }
    }
  }
}

nlohmann::json history::as_json() const {
  nlohmann::json dyn;

  for (auto const& histent : history_->entries) {
    auto& entry = dyn.emplace_back();

    auto const& version = histent.version;

    entry["libdwarfs_version"] = {
        {"major", version.major},
        {"minor", version.minor},
        {"patch", version.patch},
        {"is_release", version.is_release},
    };

    auto& version_dyn = entry["libdwarfs_version"];

    if (!version.git_rev.empty()) {
      version_dyn["git_rev"] = version.git_rev;
    }

    if (!version.git_branch.empty()) {
      version_dyn["git_branch"] = version.git_branch;
    }

    if (!version.git_desc.empty()) {
      version_dyn["git_desc"] = version.git_desc;
    }

    entry["system_id"] = histent.system_id;
    entry["compiler_id"] = histent.compiler_id;

    if (histent.arguments.has_value()) {
      auto& args = entry["arguments"];
      for (auto const& arg : histent.arguments.value()) {
        args.push_back(arg);
      }
    }

    if (histent.timestamp.has_value()) {
      entry["timestamp"] = {
          {"epoch", histent.timestamp.value()},
          {"local", fmt::format("{:%FT%T}", safe_localtime(histent.timestamp.value()))},
      };
    }

    if (histent.library_versions.has_value()) {
      auto& libs = entry["library_versions"].emplace_back();
      for (auto const& lib : histent.library_versions.value()) {
        libs.push_back(lib);
      }
    }
  }

  return dyn;
}

} // namespace dwarfs
