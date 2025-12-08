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

#include <fmt/chrono.h>
#include <fmt/format.h>

#ifdef DWARFS_HAVE_THRIFT
#include <thrift/lib/cpp2/protocol/Serializer.h>
#include <dwarfs/gen-cpp2/history_types.h>
#endif

#ifdef DWARFS_HAVE_FLATBUFFERS
#include <flatbuffers/flatbuffers.h>
#include <dwarfs/gen-flatbuffers/history_generated.h>
#endif

#ifdef DWARFS_HAVE_CEREAL
#include <cereal/cereal.hpp>
#include <cereal/archives/binary.hpp>
#include <cereal/types/optional.hpp>
#include <cereal/types/set.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>
#endif

#include <dwarfs/config.h>
#include <dwarfs/history.h>
#include <dwarfs/library_dependencies.h>
#include <dwarfs/malloc_byte_buffer.h>
#include <dwarfs/util.h>
#include <dwarfs/version.h>

namespace dwarfs {

#ifndef DWARFS_HAVE_THRIFT
// Cereal serialization support for plain structures
namespace history_internal {

#ifdef DWARFS_HAVE_CEREAL
template <class Archive>
void serialize(Archive& ar, dwarfs_version& v) {
  ar(cereal::make_nvp("major", v.major),
     cereal::make_nvp("minor", v.minor),
     cereal::make_nvp("patch", v.patch),
     cereal::make_nvp("is_release", v.is_release),
     cereal::make_nvp("git_rev", v.git_rev),
     cereal::make_nvp("git_branch", v.git_branch),
     cereal::make_nvp("git_desc", v.git_desc));
}

template <class Archive>
void serialize(Archive& ar, history_entry& e) {
  ar(cereal::make_nvp("version", e.version),
     cereal::make_nvp("system_id", e.system_id),
     cereal::make_nvp("compiler_id", e.compiler_id),
     cereal::make_nvp("arguments", e.arguments),
     cereal::make_nvp("timestamp", e.timestamp),
     cereal::make_nvp("library_versions", e.library_versions));
}

template <class Archive>
void serialize(Archive& ar, history_data& h) {
  ar(cereal::make_nvp("entries", h.entries));
}
#endif // DWARFS_HAVE_CEREAL

} // namespace history_internal
#endif // !DWARFS_HAVE_THRIFT

history::history(history_config const& cfg)
#ifdef DWARFS_HAVE_THRIFT
    : history_{std::make_unique<thrift::history::history>()}
#else
    : history_{std::make_unique<history_internal::history_data>()}
#endif
    , cfg_{cfg} {}

history::history(history&&) noexcept = default;
history::~history() noexcept = default;
history& history::operator=(history&&) noexcept = default;

void history::parse(std::span<uint8_t const> data) {
#ifdef DWARFS_HAVE_THRIFT
  history_->entries()->clear();
#else
  history_->entries.clear();
#endif
  parse_append(data);
}

void history::parse_append(std::span<uint8_t const> data) {
#ifdef DWARFS_HAVE_THRIFT
  folly::Range<uint8_t const*> range{data.data(), data.size()};
  thrift::history::history tmp;
  apache::thrift::CompactSerializer::deserialize(range, tmp);
  if (!cfg_.with_timestamps) {
    for (auto& entry : *tmp.entries()) {
      entry.timestamp().reset();
    }
  }
  history_->entries()->insert(history_->entries()->end(),
                              std::make_move_iterator(tmp.entries()->begin()),
                              std::make_move_iterator(tmp.entries()->end()));
#else
#ifdef DWARFS_HAVE_FLATBUFFERS
  // FlatBuffers deserialization
  auto fb_history = dwarfs::flatbuffers::history::GetHistory(data.data());
  if (!fb_history || !fb_history->entries()) {
    throw std::runtime_error("history::parse_append: invalid FlatBuffers data");
  }
  
  for (auto const* fb_entry : *fb_history->entries()) {
    if (!fb_entry || !fb_entry->version()) {
      continue;
    }
    
    auto& entry = history_->entries.emplace_back();
    
    // Parse version
    auto const* fb_version = fb_entry->version();
    entry.version.major = fb_version->major();
    entry.version.minor = fb_version->minor();
    entry.version.patch = fb_version->patch();
    entry.version.is_release = fb_version->is_release();
    if (fb_version->git_rev() && fb_version->git_rev()->size() > 0) {
      entry.version.git_rev = fb_version->git_rev()->str();
    }
    if (fb_version->git_branch() && fb_version->git_branch()->size() > 0) {
      entry.version.git_branch = fb_version->git_branch()->str();
    }
    if (fb_version->git_desc() && fb_version->git_desc()->size() > 0) {
      entry.version.git_desc = fb_version->git_desc()->str();
    }
    
    // Parse system and compiler IDs
    if (fb_entry->system_id()) {
      entry.system_id = fb_entry->system_id()->str();
    }
    if (fb_entry->compiler_id()) {
      entry.compiler_id = fb_entry->compiler_id()->str();
    }
    
    // Parse arguments
    if (fb_entry->arguments() && fb_entry->arguments()->size() > 0) {
      entry.arguments = std::vector<std::string>();
      for (auto const* arg : *fb_entry->arguments()) {
        if (arg) {
          entry.arguments->push_back(arg->str());
        }
      }
    }
    
    // Parse timestamp
    if (cfg_.with_timestamps && fb_entry->timestamp() != 0) {
      entry.timestamp = fb_entry->timestamp();
    }
    
    // Parse library versions
    if (fb_entry->library_versions() && fb_entry->library_versions()->size() > 0) {
      entry.library_versions = std::set<std::string>();
      for (auto const* lib : *fb_entry->library_versions()) {
        if (lib) {
          entry.library_versions->insert(lib->str());
        }
      }
    }
  }
#else
#ifdef DWARFS_HAVE_CEREAL
  history_internal::history_data tmp;
  std::string str(reinterpret_cast<char const*>(data.data()), data.size());
  std::istringstream is(str);
  cereal::BinaryInputArchive ar(is);
  ar(tmp);
  if (!cfg_.with_timestamps) {
    for (auto& entry : tmp.entries) {
      entry.timestamp.reset();
    }
  }
  history_->entries.insert(history_->entries.end(),
                           std::make_move_iterator(tmp.entries.begin()),
                           std::make_move_iterator(tmp.entries.end()));
#else
  throw std::runtime_error(
      "history::parse_append: no serialization support available");
#endif
#endif
#endif
}

void history::append(
    std::optional<std::vector<std::string>> args,
    std::function<void(library_dependencies&)> const& extra_deps) {
#ifdef DWARFS_HAVE_THRIFT
  auto& histent = history_->entries()->emplace_back();
  auto& version = histent.version().value();
  version.major() = DWARFS_VERSION_MAJOR;
  version.minor() = DWARFS_VERSION_MINOR;
  version.patch() = DWARFS_VERSION_PATCH;
  version.is_release() = std::string_view(DWARFS_GIT_DESC) == DWARFS_GIT_ID;
  version.git_rev() = DWARFS_GIT_REV;
  version.git_branch() = DWARFS_GIT_BRANCH;
  version.git_desc() = DWARFS_GIT_DESC;
  histent.system_id() = DWARFS_SYSTEM_ID;
  histent.compiler_id() = DWARFS_COMPILER_ID;
  if (args) {
    histent.arguments() = std::move(*args);
  }
  if (cfg_.with_timestamps) {
    histent.timestamp() = std::time(nullptr);
  }
  library_dependencies deps;
  deps.add_common_libraries();
  if (extra_deps) {
    extra_deps(deps);
  }
  histent.library_versions() = deps.as_set();
#else
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
#endif
}

size_t history::size() const {
#ifdef DWARFS_HAVE_THRIFT
  return history_->entries()->size();
#else
  return history_->entries.size();
#endif
}

shared_byte_buffer history::serialize() const {
#ifdef DWARFS_HAVE_THRIFT
  std::string buf;
  ::apache::thrift::CompactSerializer::serialize(*history_, &buf);
  return malloc_byte_buffer::create(buf).share();
#else
#ifdef DWARFS_HAVE_FLATBUFFERS
  // FlatBuffers serialization (highest priority for non-Thrift builds)
  ::flatbuffers::FlatBufferBuilder builder;
  
  std::vector<::flatbuffers::Offset<dwarfs::flatbuffers::history::HistoryEntry>> fb_entries;
  
  for (auto const& entry : history_->entries) {
    // Build version
    auto fb_version = dwarfs::flatbuffers::history::CreateDwarfsVersion(
      builder,
      entry.version.major,
      entry.version.minor,
      entry.version.patch,
      entry.version.is_release,
      entry.version.git_rev ? builder.CreateString(*entry.version.git_rev) : 0,
      entry.version.git_branch ? builder.CreateString(*entry.version.git_branch) : 0,
      entry.version.git_desc ? builder.CreateString(*entry.version.git_desc) : 0
    );
    
    // Build arguments
    std::vector<::flatbuffers::Offset<::flatbuffers::String>> fb_args;
    if (entry.arguments.has_value()) {
      for (auto const& arg : *entry.arguments) {
        fb_args.push_back(builder.CreateString(arg));
      }
    }
    
    // Build library versions
    std::vector<::flatbuffers::Offset<::flatbuffers::String>> fb_libs;
    if (entry.library_versions.has_value()) {
      for (auto const& lib : *entry.library_versions) {
        fb_libs.push_back(builder.CreateString(lib));
      }
    }
    
    // Build entry
    auto fb_entry = dwarfs::flatbuffers::history::CreateHistoryEntry(
      builder,
      fb_version,
      builder.CreateString(entry.system_id),
      builder.CreateString(entry.compiler_id),
      fb_args.empty() ? 0 : builder.CreateVector(fb_args),
      entry.timestamp.value_or(0),
      fb_libs.empty() ? 0 : builder.CreateVector(fb_libs)
    );
    
    fb_entries.push_back(fb_entry);
  }
  
  auto fb_history = dwarfs::flatbuffers::history::CreateHistory(
    builder,
    builder.CreateVector(fb_entries)
  );
  
  builder.Finish(fb_history);
  
  auto buf_ptr = builder.GetBufferPointer();
  auto buf_size = builder.GetSize();
  return malloc_byte_buffer::create(
    std::span<uint8_t const>(buf_ptr, buf_size)
  ).share();
#else
#ifdef DWARFS_HAVE_CEREAL
  std::ostringstream os;
  cereal::BinaryOutputArchive ar(os);
  ar(*history_);
  std::string buf = os.str();
  return malloc_byte_buffer::create(buf).share();
#else
  throw std::runtime_error(
      "history::serialize: no serialization support available");
#endif
#endif
#endif
}

void history::dump(std::ostream& os) const {
#ifdef DWARFS_HAVE_THRIFT
  auto const& entries = *history_->entries();
#else
  auto const& entries = history_->entries;
#endif

  if (!entries.empty()) {
    size_t const iwidth{std::to_string(entries.size()).size()};
    size_t i{1};

    os << "History:\n";
    for (auto const& histent : entries) {
      os << "  " << fmt::format("{:>{}}:", i++, iwidth);

#ifdef DWARFS_HAVE_THRIFT
      if (auto ts = histent.timestamp(); ts.has_value()) {
        os << " [" << fmt::format("{:%F %T}", safe_localtime(ts.value()))
           << "]";
      }
      auto const& version = histent.version().value();
      os << " libdwarfs " << version.git_desc().value();
      if (!version.is_release().value()) {
        os << " (" << version.git_branch().value() << ")";
      }
      os << " on " << histent.system_id().value() << ", "
         << histent.compiler_id().value() << "\n";
      if (histent.arguments().has_value() && !histent.arguments()->empty()) {
        os << fmt::format("  {:>{}}  args:", "", iwidth);
        for (auto const& arg : histent.arguments().value()) {
          os << ' ' << arg;
        }
        os << "\n";
      }
#else
      if (histent.timestamp.has_value()) {
        os << " [" << fmt::format("{:%F %T}", safe_localtime(histent.timestamp.value()))
           << "]";
      }
      auto const& version = histent.version;
      os << " libdwarfs " << version.git_desc.value_or("<unknown>");
      if (!version.is_release) {
        os << " (" << version.git_branch.value_or("<unknown>") << ")";
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
#endif
    }
  }
}

nlohmann::json history::as_json() const {
  nlohmann::json dyn;

#ifdef DWARFS_HAVE_THRIFT
  auto const& entries = *history_->entries();
#else
  auto const& entries = history_->entries;
#endif

  for (auto const& histent : entries) {
    auto& entry = dyn.emplace_back();

#ifdef DWARFS_HAVE_THRIFT
    auto const& version = histent.version().value();
    entry["libdwarfs_version"] = {
        {"major", version.major().value()},
        {"minor", version.minor().value()},
        {"patch", version.patch().value()},
        {"is_release", version.is_release().value()},
    };
    auto& version_dyn = entry["libdwarfs_version"];
    if (version.git_rev().has_value()) {
      version_dyn["git_rev"] = version.git_rev().value();
    }
    if (version.git_branch().has_value()) {
      version_dyn["git_branch"] = version.git_branch().value();
    }
    if (version.git_desc().has_value()) {
      version_dyn["git_desc"] = version.git_desc().value();
    }
    entry["system_id"] = histent.system_id().value();
    entry["compiler_id"] = histent.compiler_id().value();
    if (histent.arguments().has_value()) {
      auto& args = entry["arguments"];
      for (auto const& arg : histent.arguments().value()) {
        args.push_back(arg);
      }
    }
    if (auto ts = histent.timestamp(); ts.has_value()) {
      entry["timestamp"] = {
          {"epoch", ts.value()},
          {"local", fmt::format("{:%FT%T}", safe_localtime(ts.value()))},
      };
    }
    if (histent.library_versions().has_value()) {
      auto& libs = entry["library_versions"].emplace_back();
      for (auto const& lib : histent.library_versions().value()) {
        libs.push_back(lib);
      }
    }
#else
    auto const& version = histent.version;
    entry["libdwarfs_version"] = {
        {"major", version.major},
        {"minor", version.minor},
        {"patch", version.patch},
        {"is_release", version.is_release},
    };
    auto& version_dyn = entry["libdwarfs_version"];
    if (version.git_rev.has_value()) {
      version_dyn["git_rev"] = version.git_rev.value();
    }
    if (version.git_branch.has_value()) {
      version_dyn["git_branch"] = version.git_branch.value();
    }
    if (version.git_desc.has_value()) {
      version_dyn["git_desc"] = version.git_desc.value();
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
#endif
  }

  return dyn;
}

} // namespace dwarfs
