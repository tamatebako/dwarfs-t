#!/usr/bin/env python3
"""
DwarFS Benchmark Result Collector

Unified metric collection, storage, and analysis for comprehensive benchmarking.
"""

import json
import time
from pathlib import Path
from typing import Dict, List, Optional, Any
from dataclasses import dataclass, asdict, field
from datetime import datetime


@dataclass
class BenchmarkMetrics:
    """Metrics from a single benchmark run"""
    time_wall: float = 0.0  # Wall clock time (seconds)
    time_user: float = 0.0  # User CPU time
    time_sys: float = 0.0   # System CPU time
    memory_peak_mb: float = 0.0  # Peak RSS in MB
    throughput_mb_s: float = 0.0  # Throughput (MB/s)
    size_input: int = 0  # Input size (bytes)
    size_output: int = 0  # Output size (bytes)
    compression_ratio: float = 0.0  # Compression ratio
    extra: Dict[str, Any] = field(default_factory=dict)  # Additional metrics


@dataclass
class BenchmarkRun:
    """Single benchmark run result"""
    operation: str  # 'create', 'extract_full', 'extract_single', 'read_throughput', 'memory'
    build_config: str  # 'fb-only', 'thrift-only', 'both'
    dataset: str  # Dataset name
    interface: str  # 'cli' or 'api'
    format: str  # 'flatbuffers' or 'thrift'
    run_number: int  # Run number (1, 2, 3, ...)
    success: bool  # Whether run succeeded
    error: Optional[str]  # Error message if failed
    metrics: BenchmarkMetrics
    timestamp: float = field(default_factory=time.time)
    
    def to_dict(self) -> Dict:
        """Convert to dictionary for JSON serialization"""
        result = asdict(self)
        result['metrics'] = asdict(self.metrics)
        return result


class ResultCollector:
    """
    Collects and manages benchmark results
    
    Single Responsibility: Result collection, storage, and comparison
    """
    
    def __init__(self, output_dir: Path, version: str = "0.16.0"):
        """
        Initialize result collector
        
        Args:
            output_dir: Directory to store results
            version: DwarFS version being benchmarked
        """
        self.output_dir = Path(output_dir)
        self.output_dir.mkdir(parents=True, exist_ok=True)
        
        self.version = version
        self.runs: List[BenchmarkRun] = []
        
        # Metadata
        self.metadata = {
            'version': version,
            'start_time': datetime.now().isoformat(),
            'platform': self._get_platform_info(),
        }
    
    def _get_platform_info(self) -> Dict:
        """Get platform information"""
        import platform
        import os
        
        return {
            'system': platform.system(),
            'machine': platform.machine(),
            'processor': platform.processor(),
            'python_version': platform.python_version(),
            'cpu_count': os.cpu_count(),
        }
    
    def collect_operation(self,
                         operation: str,
                         build_config: str,
                         dataset: str,
                         interface: str,
                         format: str,
                         run_number: int,
                         metrics: BenchmarkMetrics,
                         success: bool = True,
                         error: Optional[str] = None) -> None:
        """
        Collect metrics from one operation
        
        Args:
            operation: Operation name
            build_config: Build configuration
            dataset: Dataset name
            interface: Interface type (cli/api)
            format: Metadata format
            run_number: Run number
            metrics: Collected metrics
            success: Whether operation succeeded
            error: Error message if failed
        """
        run = BenchmarkRun(
            operation=operation,
            build_config=build_config,
            dataset=dataset,
            interface=interface,
            format=format,
            run_number=run_number,
            success=success,
            error=error,
            metrics=metrics
        )
        
        self.runs.append(run)
    
    def get_runs(self,
                operation: Optional[str] = None,
                build_config: Optional[str] = None,
                dataset: Optional[str] = None,
                format: Optional[str] = None) -> List[BenchmarkRun]:
        """
        Filter runs by criteria
        
        Args:
            operation: Filter by operation
            build_config: Filter by build config
            dataset: Filter by dataset
            format: Filter by format
            
        Returns:
            Filtered list of runs
        """
        filtered = self.runs
        
        if operation:
            filtered = [r for r in filtered if r.operation == operation]
        if build_config:
            filtered = [r for r in filtered if r.build_config == build_config]
        if dataset:
            filtered = [r for r in filtered if r.dataset == dataset]
        if format:
            filtered = [r for r in filtered if r.format == format]
        
        return filtered
    
    def save_results(self, output_file: str = 'results.json') -> Path:
        """
        Save all results to JSON
        
        Args:
            output_file: Output filename
            
        Returns:
            Path to saved file
        """
        output_path = self.output_dir / output_file
        
        data = {
            'metadata': self.metadata,
            'metadata_end_time': datetime.now().isoformat(),
            'total_runs': len(self.runs),
            'successful_runs': sum(1 for r in self.runs if r.success),
            'failed_runs': sum(1 for r in self.runs if not r.success),
            'runs': [r.to_dict() for r in self.runs]
        }
        
        with open(output_path, 'w') as f:
            json.dump(data, f, indent=2)
        
        print(f"\n✓ Results saved: {output_path}")
        return output_path
    
    def save_detailed_results(self) -> Path:
        """
        Save detailed results with per-run breakdowns
        
        Returns:
            Path to detailed results file
        """
        detailed_path = self.output_dir / 'detailed.json'
        
        # Group by operation, build_config, dataset, format
        grouped: Dict[str, List[BenchmarkRun]] = {}
        
        for run in self.runs:
            key = f"{run.operation}_{run.build_config}_{run.dataset}_{run.format}"
            if key not in grouped:
                grouped[key] = []
            grouped[key].append(run)
        
        # Create detailed breakdown
        details = {}
        for key, runs in grouped.items():
            details[key] = {
                'runs': [r.to_dict() for r in runs],
                'count': len(runs),
                'successful': sum(1 for r in runs if r.success),
                'failed': sum(1 for r in runs if not r.success),
            }
        
        data = {
            'metadata': self.metadata,
            'groups': details
        }
        
        with open(detailed_path, 'w') as f:
            json.dump(data, f, indent=2)
        
        print(f"✓ Detailed results saved: {detailed_path}")
        return detailed_path
    
    def save_summary(self) -> Path:
        """
        Save high-level summary
        
        Returns:
            Path to summary file
        """
        summary_path = self.output_dir / 'summary.json'
        
        # Calculate summary statistics
        from .benchmark_statistics import BenchmarkStatistics
        stats = BenchmarkStatistics()
        
        summary = {
            'metadata': self.metadata,
            'version': self.version,
            'total_runs': len(self.runs),
            'successful_runs': sum(1 for r in self.runs if r.success),
            'failed_runs': sum(1 for r in self.runs if not r.success),
            'operations': {},
            'formats': {},
            'build_configs': {},
        }
        
        # Group by operation
        operations = set(r.operation for r in self.runs)
        for op in operations:
            op_runs = [r for r in self.runs if r.operation == op and r.success]
            if op_runs:
                times = [r.metrics.time_wall for r in op_runs]
                summary['operations'][op] = {
                    'count': len(op_runs),
                    'time_stats': stats.calculate_stats(times)
                }
        
        # Group by format
        formats = set(r.format for r in self.runs)
        for fmt in formats:
            fmt_runs = [r for r in self.runs if r.format == fmt and r.success]
            if fmt_runs:
                times = [r.metrics.time_wall for r in fmt_runs]
                sizes = [r.metrics.size_output for r in fmt_runs if r.metrics.size_output > 0]
                summary['formats'][fmt] = {
                    'count': len(fmt_runs),
                    'time_stats': stats.calculate_stats(times),
                    'avg_size_mb': sum(sizes) / len(sizes) / 1024 / 1024 if sizes else 0
                }
        
        # Group by build config
        configs = set(r.build_config for r in self.runs)
        for config in configs:
            config_runs = [r for r in self.runs if r.build_config == config]
            summary['build_configs'][config] = {
                'total': len(config_runs),
                'successful': sum(1 for r in config_runs if r.success),
                'failed': sum(1 for r in config_runs if not r.success),
            }
        
        with open(summary_path, 'w') as f:
            json.dump(summary, f, indent=2)
        
        print(f"✓ Summary saved: {summary_path}")
        return summary_path
    
    def compare_with_baseline(self, baseline_path: Path, 
                              threshold: float = 0.05) -> Dict:
        """
        Compare results against baseline
        
        Args:
            baseline_path: Path to baseline results JSON
            threshold: Regression threshold (5% default)
            
        Returns:
            Comparison results with regression flags
        """
        if not baseline_path.exists():
            print(f"WARNING: Baseline not found: {baseline_path}")
            return {'status': 'no_baseline'}
        
        # Load baseline
        with open(baseline_path) as f:
            baseline = json.load(f)
        
        from .benchmark_statistics import BenchmarkStatistics
        stats = BenchmarkStatistics()
        
        regressions = []
        improvements = []
        
        # Compare by operation + format
        for op in set(r.operation for r in self.runs):
            for fmt in set(r.format for r in self.runs):
                current_runs = [
                    r for r in self.runs 
                    if r.operation == op and r.format == fmt and r.success
                ]
                
                if not current_runs:
                    continue
                
                current_time = sum(r.metrics.time_wall for r in current_runs) / len(current_runs)
                
                # Find baseline
                baseline_key = f"{op}_{fmt}"
                baseline_runs = baseline.get('runs', [])
                baseline_times = [
                    r['metrics']['time_wall'] 
                    for r in baseline_runs 
                    if r['operation'] == op and r['format'] == fmt and r['success']
                ]
                
                if not baseline_times:
                    continue
                
                baseline_time = sum(baseline_times) / len(baseline_times)
                
                # Check for regression
                if stats.detect_regression(current_time, baseline_time, threshold):
                    regressions.append({
                        'operation': op,
                        'format': fmt,
                        'current': current_time,
                        'baseline': baseline_time,
                        'delta_percent': ((current_time - baseline_time) / baseline_time) * 100
                    })
                elif current_time < baseline_time * (1 - threshold):
                    improvements.append({
                        'operation': op,
                        'format': fmt,
                        'current': current_time,
                        'baseline': baseline_time,
                        'delta_percent': ((baseline_time - current_time) / baseline_time) * 100
                    })
        
        comparison = {
            'status': 'compared',
            'baseline_version': baseline.get('metadata', {}).get('version', 'unknown'),
            'current_version': self.version,
            'threshold_percent': threshold * 100,
            'regressions': regressions,
            'improvements': improvements,
            'has_regressions': len(regressions) > 0,
        }
        
        # Save comparison
        comparison_path = self.output_dir / 'regression.json'
        with open(comparison_path, 'w') as f:
            json.dump(comparison, f, indent=2)
        
        print(f"✓ Regression analysis saved: {comparison_path}")
        
        if regressions:
            print(f"\n⚠ WARNING: {len(regressions)} regression(s) detected!")
            for reg in regressions:
                print(f"  {reg['operation']} ({reg['format']}): "
                      f"+{reg['delta_percent']:.1f}% slower")
        
        if improvements:
            print(f"\n✓ {len(improvements)} improvement(s) detected!")
            for imp in improvements:
                print(f"  {imp['operation']} ({imp['format']}): "
                      f"-{imp['delta_percent']:.1f}% faster")
        
        return comparison
    
    def print_summary(self) -> None:
        """Print human-readable summary to console"""
        print("\n" + "="*70)
        print("BENCHMARK RESULTS SUMMARY")
        print("="*70)
        print(f"Version: {self.version}")
        print(f"Total runs: {len(self.runs)}")
        print(f"Successful: {sum(1 for r in self.runs if r.success)}")
        print(f"Failed: {sum(1 for r in self.runs if not r.success)}")
        print()
        
        # Group by operation
        operations = set(r.operation for r in self.runs)
        for op in sorted(operations):
            op_runs = [r for r in self.runs if r.operation == op and r.success]
            if op_runs:
                times = [r.metrics.time_wall for r in op_runs]
                avg_time = sum(times) / len(times)
                print(f"{op}:")
                print(f"  Runs: {len(op_runs)}")
                print(f"  Avg time: {avg_time:.3f}s")
        
        print("="*70 + "\n")


def main():
    """CLI interface for result collector"""
    import argparse
    
    parser = argparse.ArgumentParser(
        description='DwarFS Benchmark Result Collector')
    parser.add_argument('--output-dir', type=Path, required=True,
                       help='Output directory for results')
    parser.add_argument('--compare-baseline', type=Path,
                       help='Compare against baseline results')
    parser.add_argument('--threshold', type=float, default=0.05,
                       help='Regression threshold (default: 5%%)')
    
    args = parser.parse_args()
    
    collector = ResultCollector(args.output_dir)
    
    if args.compare_baseline:
        collector.compare_with_baseline(args.compare_baseline, args.threshold)
    
    return 0


if __name__ == '__main__':
    exit(main())