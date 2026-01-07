//
// Copyright (c) Marcus Holland-Moritz
//
// This file is part of dwarfs.
//
// dwarfs is free software: you can redistribute it and/or modify it under the
// terms of the GNU General Public License as published by the Free Software
// Foundation, either version 3 of the License, or (at your option) any later
// version.
//
// dwarfs is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
// A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along with
// dwarfs.  If not, see <https://www.gnu.org/licenses/>.
//

#include "dwarfs/tool/version_info.h"

#include <fmt/format.h>

#include <boost/version.hpp>
#include <openssl/opensslv.h>
#include <xxhash.h>
#include <zstd.h>

#include "dwarfs/config.h"
#include "dwarfs/version.h"

#ifdef DWARFS_HAVE_LIBLZ4
#include <lz4.h>
#endif

#ifdef DWARFS_HAVE_LIBLZMA
#include <lzma.h>
#endif

#ifdef DWARFS_HAVE_LIBBROTLI
#include <brotli/encode.h>
#endif

#ifdef DWARFS_HAVE_FLAC
#include <FLAC/format.h>
#endif

namespace dwarfs::tool {

namespace {

std::string get_openssl_version() {
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
  return OPENSSL_VERSION_STR;
#else
  return OPENSSL_VERSION_TEXT;
#endif
}

} // namespace

version_info version_info::get() {
  version_info info;

  // Version information from generated version.h
  info.version = DWARFS_GIT_DESC;
  info.version_short = fmt::format("{}.{}.{}",
    DWARFS_VERSION_MAJOR, DWARFS_VERSION_MINOR, DWARFS_VERSION_PATCH);
  info.commit_hash = DWARFS_GIT_REV;
  info.commit_date = DWARFS_GIT_DATE ? DWARFS_GIT_DATE : "unknown";
  info.git_branch = DWARFS_GIT_BRANCH;
  info.is_release = std::string(DWARFS_GIT_BRANCH) == "HEAD" ||
                    std::string(DWARFS_GIT_BRANCH).find("release") != std::string::npos;

  // Build information
#ifdef DWARFS_BUILD_ID
  info.build_id = DWARFS_BUILD_ID;
#else
  info.build_id = "unknown";
#endif

  info.compiler = get_compiler_version();

#ifdef __DATE__
  info.build_date = __DATE__;
#ifdef __TIME__
  info.build_date += " ";
  info.build_date += __TIME__;
#endif
#else
  info.build_date = "unknown";
#endif

#ifdef CMAKE_BUILD_TYPE
  info.build_type = CMAKE_BUILD_TYPE;
#elif defined(NDEBUG)
  info.build_type = "Release";
#else
  info.build_type = "Debug";
#endif

  // Platform information
  info.platform = get_platform_string();

#if defined(__x86_64__) || defined(_M_X64)
  info.architecture = "x86_64";
#elif defined(__aarch64__) || defined(_M_ARM64)
  info.architecture = "aarch64";
#elif defined(__i386__) || defined(_M_IX86)
  info.architecture = "i386";
#elif defined(__arm__) || defined(_M_ARM)
  info.architecture = "arm";
#elif defined(__ppc64__) || defined(__powerpc64__)
  info.architecture = "ppc64";
#elif defined(__riscv) && (__riscv_xlen == 64)
  info.architecture = "riscv64";
#elif defined(__s390x__)
  info.architecture = "s390x";
#elif defined(__loongarch64)
  info.architecture = "loongarch64";
#else
  info.architecture = "unknown";
#endif

  // Features and libraries
  info.features = get_feature_list();
  info.libraries = get_library_versions();

  return info;
}

std::string version_info::get_compiler_version() {
#if defined(__clang__)
  return fmt::format("clang {}.{}.{}",
    __clang_major__, __clang_minor__, __clang_patchlevel__);
#elif defined(__GNUC__)
  return fmt::format("GCC {}.{}.{}",
    __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__);
#elif defined(_MSC_VER)
  return fmt::format("MSVC {}", _MSC_VER);
#else
  return "unknown compiler";
#endif
}

std::string version_info::get_platform_string() {
#if defined(__linux__)
  return "Linux";
#elif defined(__APPLE__)
  #if defined(__arm64__) || defined(__aarch64__)
    return "macOS ARM64";
  #else
    return "macOS";
  #endif
#elif defined(_WIN32)
  #if defined(_WIN64)
    return "Windows 64-bit";
  #else
    return "Windows 32-bit";
  #endif
#elif defined(__FreeBSD__)
  return "FreeBSD";
#elif defined(__NetBSD__)
  return "NetBSD";
#elif defined(__OpenBSD__)
  return "OpenBSD";
#else
  return "Unknown OS";
#endif
}

std::vector<std::string> version_info::get_feature_list() {
  std::vector<std::string> features;

#ifdef DWARFS_HAVE_FLATBUFFERS
  features.push_back("FlatBuffers");
#endif

#ifdef DWARFS_HAVE_THRIFT
  features.push_back("Thrift");
#endif

#ifdef DWARFS_HAVE_FLAC
  features.push_back("FLAC");
#endif

#ifdef DWARFS_HAVE_LIBLZ4
  features.push_back("LZ4");
#endif

#ifdef DWARFS_HAVE_LIBLZMA
  features.push_back("LZMA");
#endif

#ifdef DWARFS_HAVE_LIBBROTLI
  features.push_back("Brotli");
#endif

#ifdef DWARFS_HAVE_RICEPP
  features.push_back("Rice++");
#endif

#ifdef DWARFS_PERFMON_ENABLED
  features.push_back("PerfMon");
#endif

#ifdef DWARFS_USE_JEMALLOC
  features.push_back("jemalloc");
#endif

#ifdef DWARFS_USE_MIMALLOC
  features.push_back("mimalloc");
#endif

#ifdef DWARFS_BUILTIN_MANPAGE
  features.push_back("built-in manpage");
#endif

  return features;
}

std::map<std::string, std::string> version_info::get_library_versions() {
  std::map<std::string, std::string> libs;

  // Boost
  libs["Boost"] = fmt::format("{}.{}.{}",
    BOOST_VERSION / 100000,
    (BOOST_VERSION / 100) % 1000,
    BOOST_VERSION % 100);

  // OpenSSL
  libs["OpenSSL"] = get_openssl_version();

  // zstd
  libs["zstd"] = ZSTD_versionString();

  // xxHash - use macro not function
#ifdef XXH_VERSION_STRING
  libs["xxHash"] = XXH_VERSION_STRING;
#else
  libs["xxHash"] = fmt::format("{}.{}.{}",
    XXH_VERSION_NUMBER / 10000,
    (XXH_VERSION_NUMBER / 100) % 100,
    XXH_VERSION_NUMBER % 100);
#endif

#ifdef DWARFS_HAVE_LIBLZ4
  libs["LZ4"] = LZ4_versionString();
#endif

#ifdef DWARFS_HAVE_LIBLZMA
  libs["liblzma"] = lzma_version_string();
#endif

#ifdef DWARFS_HAVE_LIBBROTLI
  // Use BrotliEncoderVersion() function
  uint32_t ver = BrotliEncoderVersion();
  libs["Brotli"] = fmt::format("{}.{}.{}",
    ver >> 24,
    (ver >> 12) & 0xFFF,
    ver & 0xFFF);
#endif

#ifdef DWARFS_HAVE_FLAC
  libs["FLAC"] = FLAC__VERSION_STRING;
#endif

  return libs;
}

std::string version_info::to_string(std::string_view tool_name,
                                   bool include_features,
                                   bool include_libraries) const {
  std::string result;

  // Tool name and version
  result += fmt::format("{} {}", tool_name, version);

  // Commit info if not a release
  if (!is_release && !commit_hash.empty()) {
    result += fmt::format(" (commit: {}", commit_hash);
    if (!commit_date.empty() && commit_date != "unknown") {
      result += fmt::format(", {}", commit_date);
    }
    result += ")";
  }
  result += "\n";

  // Build information
  result += fmt::format("Compiler: {}\n", compiler);
  result += fmt::format("Platform: {} {}\n", platform, architecture);
  result += fmt::format("Build: {}\n", build_type);

  // Features
  if (include_features && !features.empty()) {
    result += "Features: ";
    for (size_t i = 0; i < features.size(); ++i) {
      if (i > 0) result += ", ";
      result += features[i];
    }
    result += "\n";
  }

  // Libraries
  if (include_libraries && !libraries.empty()) {
    result += "Libraries:\n";
    for (const auto& [name, version] : libraries) {
      result += fmt::format("  - {} {}\n", name, version);
    }
  }

  return result;
}

std::string version_info::short_string() const {
  return version_short;
}

} // namespace dwarfs::tool