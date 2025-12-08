#!/usr/bin/env python3
"""
DwarFS Benchmark Statistics Module

Statistical analysis for benchmark results including:
- Descriptive statistics (mean, median, stdev)
- Percentile calculations
- Confidence intervals
- Outlier detection (IQR method)
- Regression detection
"""

import math
import statistics as stats_stdlib
from typing import List, Dict, Tuple, Optional
from dataclasses import dataclass


@dataclass
class StatisticalSummary:
    """Statistical summary of a set of measurements"""
    count: int
    mean: float
    median: float
    stdev: float
    min: float
    max: float
    p50: float  # Same as median
    p90: float
    p95: float
    p99: float
    variance: float
    cv: float  # Coefficient of variation (stdev/mean)
    
    def to_dict(self) -> Dict:
        """Convert to dictionary"""
        return {
            'count': self.count,
            'mean': self.mean,
            'median': self.median,
            'stdev': self.stdev,
            'min': self.min,
            'max': self.max,
            'percentiles': {
                'p50': self.p50,
                'p90': self.p90,
                'p95': self.p95,
                'p99': self.p99,
            },
            'variance': self.variance,
            'cv_percent': self.cv * 100,
        }


class BenchmarkStatistics:
    """
    Statistical analysis for benchmark results
    
    Single Responsibility: Compute statistics and detect anomalies
    """
    
    @staticmethod
    def calculate_stats(values: List[float]) -> Dict:
        """
        Calculate comprehensive statistics
        
        Args:
            values: List of measurements
            
        Returns:
            Dictionary with statistical metrics
        """
        if not values:
            return {}
        
        if len(values) == 1:
            # Single value
            val = values[0]
            return {
                'count': 1,
                'mean': val,
                'median': val,
                'stdev': 0.0,
                'min': val,
                'max': val,
                'p50': val,
                'p90': val,
                'p95': val,
                'p99': val,
                'variance': 0.0,
                'cv_percent': 0.0,
            }
        
        # Sort for percentile calculations
        sorted_values = sorted(values)
        
        mean_val = stats_stdlib.mean(values)
        median_val = stats_stdlib.median(values)
        stdev_val = stats_stdlib.stdev(values) if len(values) > 1 else 0.0
        variance_val = stats_stdlib.variance(values) if len(values) > 1 else 0.0
        
        # Coefficient of variation
        cv = stdev_val / mean_val if mean_val != 0 else 0.0
        
        # Percentiles
        p50 = BenchmarkStatistics._percentile(sorted_values, 0.50)
        p90 = BenchmarkStatistics._percentile(sorted_values, 0.90)
        p95 = BenchmarkStatistics._percentile(sorted_values, 0.95)
        p99 = BenchmarkStatistics._percentile(sorted_values, 0.99)
        
        return {
            'count': len(values),
            'mean': mean_val,
            'median': median_val,
            'stdev': stdev_val,
            'min': min(values),
            'max': max(values),
            'p50': p50,
            'p90': p90,
            'p95': p95,
            'p99': p99,
            'variance': variance_val,
            'cv_percent': cv * 100,
        }
    
    @staticmethod
    def _percentile(sorted_values: List[float], p: float) -> float:
        """
        Calculate percentile from sorted values
        
        Args:
            sorted_values: Sorted list of values
            p: Percentile (0.0 to 1.0)
            
        Returns:
            Percentile value
        """
        if not sorted_values:
            return 0.0
        
        if len(sorted_values) == 1:
            return sorted_values[0]
        
        # Use linear interpolation between closest ranks
        k = (len(sorted_values) - 1) * p
        f = math.floor(k)
        c = math.ceil(k)
        
        if f == c:
            return sorted_values[int(k)]
        
        d0 = sorted_values[int(f)] * (c - k)
        d1 = sorted_values[int(c)] * (k - f)
        
        return d0 + d1
    
    @staticmethod
    def detect_outliers(values: List[float], 
                       method: str = 'iqr',
                       threshold: float = 1.5) -> Tuple[List[float], List[int]]:
        """
        Detect outliers using IQR method
        
        Args:
            values: List of measurements
            method: Detection method ('iqr' or 'zscore')
            threshold: Threshold multiplier (1.5 for IQR, 3.0 for z-score)
            
        Returns:
            Tuple of (clean_values, outlier_indices)
        """
        if len(values) < 4:
            return values, []
        
        if method == 'iqr':
            return BenchmarkStatistics._detect_outliers_iqr(values, threshold)
        elif method == 'zscore':
            return BenchmarkStatistics._detect_outliers_zscore(values, threshold)
        else:
            return values, []
    
    @staticmethod
    def _detect_outliers_iqr(values: List[float], threshold: float = 1.5) -> Tuple[List[float], List[int]]:
        """Detect outliers using Interquartile Range (IQR) method"""
        sorted_values = sorted(values)
        
        q1 = BenchmarkStatistics._percentile(sorted_values, 0.25)
        q3 = BenchmarkStatistics._percentile(sorted_values, 0.75)
        iqr = q3 - q1
        
        lower_bound = q1 - threshold * iqr
        upper_bound = q3 + threshold * iqr
        
        outlier_indices = []
        clean_values = []
        
        for i, val in enumerate(values):
            if val < lower_bound or val > upper_bound:
                outlier_indices.append(i)
            else:
                clean_values.append(val)
        
        return clean_values, outlier_indices
    
    @staticmethod
    def _detect_outliers_zscore(values: List[float], threshold: float = 3.0) -> Tuple[List[float], List[int]]:
        """Detect outliers using Z-score method"""
        if len(values) < 2:
            return values, []
        
        mean_val = stats_stdlib.mean(values)
        stdev_val = stats_stdlib.stdev(values)
        
        if stdev_val == 0:
            return values, []
        
        outlier_indices = []
        clean_values = []
        
        for i, val in enumerate(values):
            z_score = abs((val - mean_val) / stdev_val)
            if z_score > threshold:
                outlier_indices.append(i)
            else:
                clean_values.append(val)
        
        return clean_values, outlier_indices
    
    @staticmethod
    def confidence_interval(values: List[float], 
                           confidence: float = 0.95) -> Tuple[float, float]:
        """
        Calculate confidence interval for the mean
        
        Args:
            values: List of measurements
            confidence: Confidence level (default: 0.95 for 95%)
            
        Returns:
            Tuple of (lower_bound, upper_bound)
        """
        if len(values) < 2:
            if values:
                return (values[0], values[0])
            return (0.0, 0.0)
        
        mean_val = stats_stdlib.mean(values)
        stdev_val = stats_stdlib.stdev(values)
        n = len(values)
        
        # Use t-distribution for small samples
        if n < 30:
            # Approximate t-value for common confidence levels
            t_values = {
                0.90: 1.833,  # df=9
                0.95: 2.262,  # df=9
                0.99: 3.250,  # df=9
            }
            t = t_values.get(confidence, 2.262)
        else:
            # Use z-score for large samples
            z_values = {
                0.90: 1.645,
                0.95: 1.960,
                0.99: 2.576,
            }
            t = z_values.get(confidence, 1.960)
        
        margin = t * (stdev_val / math.sqrt(n))
        
        return (mean_val - margin, mean_val + margin)
    
    @staticmethod
    def detect_regression(current: float, 
                         baseline: float,
                         threshold: float = 0.05) -> bool:
        """
        Detect if current value is significantly worse than baseline
        
        Args:
            current: Current measurement
            baseline: Baseline measurement
            threshold: Regression threshold (0.05 = 5%)
            
        Returns:
            True if regression detected
        """
        if baseline == 0:
            return False
        
        # Calculate relative change
        delta = (current - baseline) / baseline
        
        # Positive delta means slower/worse for time-based metrics
        return delta > threshold
    
    @staticmethod
    def compare_distributions(values1: List[float], 
                             values2: List[float]) -> Dict:
        """
        Compare two distributions
        
        Args:
            values1: First set of measurements
            values2: Second set of measurements
            
        Returns:
            Comparison statistics
        """
        if not values1 or not values2:
            return {}
        
        stats1 = BenchmarkStatistics.calculate_stats(values1)
        stats2 = BenchmarkStatistics.calculate_stats(values2)
        
        # Calculate effect size (Cohen's d)
        mean1 = stats1['mean']
        mean2 = stats2['mean']
        
        if len(values1) > 1 and len(values2) > 1:
            pooled_stdev = math.sqrt(
                ((len(values1) - 1) * stats1['variance'] + 
                 (len(values2) - 1) * stats2['variance']) /
                (len(values1) + len(values2) - 2)
            )
            
            cohens_d = (mean1 - mean2) / pooled_stdev if pooled_stdev > 0 else 0
        else:
            cohens_d = 0
        
        return {
            'group1': stats1,
            'group2': stats2,
            'mean_delta': mean1 - mean2,
            'mean_delta_percent': ((mean1 - mean2) / mean2 * 100) if mean2 != 0 else 0,
            'median_delta': stats1['median'] - stats2['median'],
            'cohens_d': cohens_d,
            'effect_size': BenchmarkStatistics._interpret_cohens_d(cohens_d),
        }
    
    @staticmethod
    def _interpret_cohens_d(d: float) -> str:
        """Interpret Cohen's d effect size"""
        abs_d = abs(d)
        if abs_d < 0.2:
            return 'negligible'
        elif abs_d < 0.5:
            return 'small'
        elif abs_d < 0.8:
            return 'medium'
        else:
            return 'large'
    
    @staticmethod
    def is_stable(values: List[float], cv_threshold: float = 0.20) -> bool:
        """
        Check if measurements are stable (low variance)
        
        Args:
            values: List of measurements
            cv_threshold: Coefficient of variation threshold (0.20 = 20%)
            
        Returns:
            True if stable
        """
        if len(values) < 2:
            return True
        
        stats = BenchmarkStatistics.calculate_stats(values)
        cv = stats['cv_percent'] / 100
        
        return cv <= cv_threshold
    
    @staticmethod
    def summarize(values: List[float], 
                 name: str = "Metric",
                 outlier_detection: bool = True) -> Dict:
        """
        Generate comprehensive summary with outlier detection
        
        Args:
            values: List of measurements
            name: Metric name
            outlier_detection: Whether to detect outliers
            
        Returns:
            Complete summary dictionary
        """
        if not values:
            return {'name': name, 'count': 0}
        
        # Detect outliers if requested
        if outlier_detection and len(values) >= 4:
            clean_values, outlier_indices = BenchmarkStatistics.detect_outliers(values)
            has_outliers = len(outlier_indices) > 0
        else:
            clean_values = values
            outlier_indices = []
            has_outliers = False
        
        # Calculate statistics on clean values
        stats = BenchmarkStatistics.calculate_stats(clean_values)
        
        # Confidence interval
        ci_lower, ci_upper = BenchmarkStatistics.confidence_interval(clean_values)
        
        # Stability check
        stable = BenchmarkStatistics.is_stable(clean_values)
        
        return {
            'name': name,
            'raw_count': len(values),
            'clean_count': len(clean_values),
            'outliers_removed': len(outlier_indices),
            'outlier_indices': outlier_indices,
            'has_outliers': has_outliers,
            'statistics': stats,
            'confidence_interval_95': {
                'lower': ci_lower,
                'upper': ci_upper,
            },
            'stable': stable,
        }


def main():
    """Demo and testing"""
    import random
    
    print("BenchmarkStatistics Demo\n")
    
    # Generate some sample data
    random.seed(42)
    samples = [random.gauss(10.0, 1.5) for _ in range(30)]
    
    # Add an outlier
    samples.append(25.0)
    
    print(f"Sample data (n={len(samples)}):")
    print(f"  {samples[:5]}... (truncated)\n")
    
    # Calculate stats
    stats = BenchmarkStatistics()
    summary = stats.summarize(samples, "Response Time (ms)")
    
    print("Statistical Summary:")
    print(f"  Count: {summary['raw_count']} (cleaned: {summary['clean_count']})")
    print(f"  Outliers: {summary['outliers_removed']}")
    print(f"  Mean: {summary['statistics']['mean']:.3f} ms")
    print(f"  Median: {summary['statistics']['median']:.3f} ms")
    print(f"  StdDev: {summary['statistics']['stdev']:.3f} ms")
    print(f"  CV: {summary['statistics']['cv_percent']:.1f}%")
    print(f"  Stable: {summary['stable']}")
    print(f"  95% CI: [{summary['confidence_interval_95']['lower']:.3f}, "
          f"{summary['confidence_interval_95']['upper']:.3f}]")
    print(f"\n  Percentiles:")
    print(f"    p50: {summary['statistics']['p50']:.3f} ms")
    print(f"    p90: {summary['statistics']['p90']:.3f} ms")
    print(f"    p95: {summary['statistics']['p95']:.3f} ms")
    print(f"    p99: {summary['statistics']['p99']:.3f} ms")
    
    # Test regression detection
    print("\nRegression Detection:")
    baseline = 10.0
    current = 11.0
    is_regression = stats.detect_regression(current, baseline, threshold=0.05)
    print(f"  Baseline: {baseline:.3f} ms")
    print(f"  Current: {current:.3f} ms")
    print(f"  Regression (5% threshold): {is_regression}")
    
    return 0


if __name__ == '__main__':
    exit(main())