/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \author     Marcus Holland-Moritz (github@mhxnet.de)
 * \copyright  Copyright (c) Marcus Holland-Moritz
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

/**
 * \file histogram.h
 * \brief Histogram utilities (folly::Histogram replacement)
 *
 * Provides histogram data structure for statistical analysis.
 * Compatible with folly::Histogram API.
 */

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

namespace dwarfs::compat {

/**
 * \brief Histogram for collecting and analyzing value distributions
 *
 * Replacement for folly::Histogram<T>.
 * Uses logarithmic bucketing for efficient storage.
 *
 * \tparam T Value type (must be arithmetic)
 */
template <typename T>
class Histogram {
 public:
  /**
   * \brief Construct histogram with bucket configuration
   *
   * \param bucket_size Size of each bucket (not used in log bucketing)
   * \param min Minimum value
   * \param max Maximum value
   */
  Histogram(T bucket_size, T min, T max)
      : min_(min)
      , max_(max)
      , bucket_size_(bucket_size)
      , count_(0)
      , sum_(0) {
    // Use logarithmic bucketing similar to folly::Histogram
    // Create approximately 100 buckets by default
    size_t num_buckets = 100;
    buckets_.resize(num_buckets, 0);
  }

  /**
   * \brief Add a value to the histogram
   *
   * \param value Value to add
   * \return Reference to this histogram
   */
  Histogram& addValue(T value) {
    count_++;
    sum_ += value;

    // Clamp to range
    if (value < min_) value = min_;
    if (value > max_) value = max_;

    // Compute bucket index using logarithmic scale
    size_t bucket = computeBucket(value);
    if (bucket < buckets_.size()) {
      buckets_[bucket]++;
    }

    return *this;
  }

  /**
   * \brief Get count of values added
   */
  uint64_t count() const { return count_; }

  /**
   * \brief Get sum of all values
   */
  T sum() const { return sum_; }

  /**
   * \brief Get average of all values
   */
  double avg() const {
    return count_ > 0 ? static_cast<double>(sum_) / count_ : 0.0;
  }

  /**
   * \brief Estimate value at given percentile
   *
   * \param pct Percentile (0.0 to 1.0)
   * \return Estimated value at percentile
   */
  T getPercentileEstimate(double pct) const {
    if (count_ == 0) {
      return T{};
    }

    // Find bucket containing the percentile
    uint64_t target = static_cast<uint64_t>(pct * count_);
    uint64_t cumulative = 0;

    for (size_t i = 0; i < buckets_.size(); ++i) {
      cumulative += buckets_[i];
      if (cumulative >= target) {
        return bucketToValue(i);
      }
    }

    return max_;
  }

  /**
   * \brief Get minimum value
   */
  T getMin() const { return min_; }

  /**
   * \brief Get maximum value
   */
  T getMax() const { return max_; }

  /**
   * \brief Clear all data
   */
  void clear() {
    std::fill(buckets_.begin(), buckets_.end(), 0);
    count_ = 0;
    sum_ = 0;
  }

  /**
   * \brief Merge another histogram into this one
   *
   * \param other Histogram to merge
   */
  void merge(const Histogram& other) {
    if (buckets_.size() == other.buckets_.size()) {
      for (size_t i = 0; i < buckets_.size(); ++i) {
        buckets_[i] += other.buckets_[i];
      }
      count_ += other.count_;
      sum_ += other.sum_;
    }
  }

 private:
  /**
   * \brief Compute bucket index for a value
   */
  size_t computeBucket(T value) const {
    if (value <= min_) return 0;
    if (value >= max_) return buckets_.size() - 1;

    // Logarithmic bucketing
    double range = static_cast<double>(max_ - min_);
    double offset = static_cast<double>(value - min_);
    double ratio = offset / range;

    // Use log scale for better distribution
    double log_ratio = std::log1p(ratio * 99.0) / std::log(100.0);
    size_t bucket = static_cast<size_t>(log_ratio * (buckets_.size() - 1));

    return std::min(bucket, buckets_.size() - 1);
  }

  /**
   * \brief Convert bucket index back to approximate value
   */
  T bucketToValue(size_t bucket) const {
    if (bucket == 0) return min_;
    if (bucket >= buckets_.size() - 1) return max_;

    double ratio = static_cast<double>(bucket) / (buckets_.size() - 1);
    double log_ratio = std::exp(ratio * std::log(100.0)) - 1.0;
    log_ratio /= 99.0;

    double range = static_cast<double>(max_ - min_);
    return min_ + static_cast<T>(log_ratio * range);
  }

  T min_;
  T max_;
  T bucket_size_;
  uint64_t count_;
  T sum_;
  std::vector<uint64_t> buckets_;
};

} // namespace dwarfs::compat