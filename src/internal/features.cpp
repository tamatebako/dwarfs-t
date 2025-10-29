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

#include <algorithm>
#include <iterator>
#include <string_view>
#include <array>

#include <dwarfs/internal/features.h>

namespace dwarfs::internal {

namespace {

using metadata::domain::feature;

constexpr bool is_supported_feature(feature /*f*/) { return true; }

// Map of feature enum to string name
constexpr std::array<std::pair<feature, std::string_view>, 1> feature_names{{
    {feature::sparsefiles, "sparsefiles"},
}};

std::string feature_name(feature f) {
  for (auto const& [feat, name] : feature_names) {
    if (feat == f) {
      return std::string{name};
    }
  }
  return "unknown";
}

} // namespace

void feature_set::add(metadata::domain::feature f) {
  features_.insert(feature_name(f));
}

bool feature_set::has(metadata::domain::feature f) const {
  return features_.contains(feature_name(f));
}

std::set<std::string> feature_set::get_supported() {
  std::set<std::string> rv;
  for (auto const& [f, name] : feature_names) {
    if (is_supported_feature(f)) {
      rv.insert(std::string{name});
    }
  }
  return rv;
};

std::set<std::string>
feature_set::get_unsupported(std::set<std::string> const& wanted_features) {
  auto const supported_features = get_supported();
  std::set<std::string> missing;
  std::ranges::set_difference(wanted_features, supported_features,
                              std::inserter(missing, missing.end()));
  return missing;
}

void feature_set::set(std::set<std::string> const& features) {
  features_ = features;
}

} // namespace dwarfs::internal
