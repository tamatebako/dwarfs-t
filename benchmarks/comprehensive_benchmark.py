#!/usr/bin/env python3
"""
DwarFS Comprehensive Benchmark Suite

Tests multiple build configurations, datasets, and operations:
- Build configs: fb-only, thrift-only (expected to fail), both
- Operations: create, extract_full, extract_single
- Formats: FlatBuffers, Thrift
- Interfaces: CLI (primary), API (future)

Collects metrics, detects regressions, generates reports.
"""

import argparse
import subprocess
import sys
import tempfile
import time
from pathlib import Path
from typing import Dict, List, Optional, Tuple

# Add lib to path
sys.path.insert(0, str(Path(__file__).parent / 'lib'))

from lib.build_manager import BuildManager
from lib.dataset_manager import DatasetManager
from lib.result_collector import ResultCollector, BenchmarkMetrics
from lib.benchmark_statistics import BenchmarkStatistics


class ComprehensiveBenchmark:
    """
    Main benchmark orchestrator
    
    Single Responsibility: Coordinate all benchmark operations
    """
    
    def __init__(self, workspace: Path, output_dir: Path, verbose: bool = False):
        """
        Initialize comprehensive benchmark
        
        Args:
            workspace: DwarFS workspace root
            output_dir: Output directory for results
            verbose: Verbose output
        """
        self.workspace = Path(workspace).resolve()
        self.output_dir = Path(output_dir)
        self.output_dir.mkdir(parents=True, exist_ok=True)
        self.verbose = verbose
        
        # Initialize managers
        self.build_manager = BuildManager(self.workspace)
        self.dataset_manager = DatasetManager(self.workspace)
        self.collector = ResultCollector(self.output_dir)
        self.stats = BenchmarkStatistics()
        
        print(f"Workspace: {self.workspace}")
        print(f"Output: {self.output_dir}")
        print()
    
    def run_comprehensive_suite(self,
                               builds: List[str],
                               datasets: List[str],
                               operations: List[str],
                               runs_per_test: int = 3,
                               formats: Optional[List[str]] = None) -> bool:
        """
        Run complete benchmark suite
        
        Args:
            builds: Build configurations to test
            datasets: Datasets to use
            operations: Operations to perform
            runs_per_test: Runs per test (for statistical significance)
            formats: Formats to test (default: both)
            
        Returns:
            True if all succeeded
        """
        if formats is None:
            formats = ['flatbuffers', 'thrift']
        
        print(f"{'='*70}")
        print(f"COMPREHENSIVE BENCHMARK SUITE")
        print(f"{'='*70}")
        print(f"Builds: {', '.join(builds)}")
        print(f"Datasets: {', '.join(datasets)}")
        print(f"Operations: {', '.join(operations)}")
        print(f"Formats: {', '.join(formats)}")
        print(f"Runs per test: {runs_per_test}")
        print(f"{'='*70}\n")
        
        # Phase 1: Build all configurations
        print("\n" + "="*70)
        print("PHASE 1: BUILD CONFIGURATIONS")
        print("="*70 + "\n")
        
        built = {}
        for build in builds:
            result = self.build_manager.build_config(
                build,
                verbose=self.verbose
            )
            if result:
                built[build] = result
                print(f"✓ {build}: {result}\n")
            else:
                if self.build_manager.CONFIGS[build].expected_to_fail:
                    print(f"✓ {build}: Failed as EXPECTED\n")
                else:
                    print(f"✗ {build}: Build FAILED\n")
                    return False
        
        # Phase 2: Prepare datasets
        print("\n" + "="*70)
        print("PHASE 2: PREPARE DATASETS")
        print("="*70 + "\n")
        
        prepared_datasets = {}
        for dataset in datasets:
            path = self.dataset_manager.prepare_dataset(dataset)
            if path:
                prepared_datasets[dataset] = path
                print(f"✓ {dataset}: {path}\n")
            else:
                print(f"⚠ {dataset}: Not available, skipping\n")
        
        if not prepared_datasets:
            print("ERROR: No datasets available")
            return False
        
        # Phase 3: Run benchmarks
        print("\n" + "="*70)
        print("PHASE 3: RUN BENCHMARKS")
        print("="*70 + "\n")
        
        total_tests = 0
        successful_tests = 0
        
        for build in built.keys():
            for dataset_name, dataset_path in prepared_datasets.items():
                for operation in operations:
                    for format in formats:
                        # Check if this combination makes sense
                        config = self.build_manager.CONFIGS[build]
                        
                        # For creation, we can only write formats we support
                        if operation == 'create' and format not in config.can_write:
                            print(f"SKIP: {build} cannot write {format} (expected)")
                            continue
                        
                        # For reading, we can only read formats we support
                        if operation in ['extract_full', 'extract_single'] and format not in config.can_read:
                            print(f"SKIP: {build} cannot read {format} (expected)")
                            continue
                        
                        # Run multiple times for statistics
                        for run in range(1, runs_per_test + 1):
                            total_tests += 1
                            
                            print(f"\n[{total_tests}] {build} | {dataset_name} | {operation} | {format} | run {run}/{runs_per_test}")
                            
                            success = self._run_operation(
                                build=build,
                                dataset_name=dataset_name,
                                dataset_path=dataset_path,
                                operation=operation,
                                format=format,
                                run_number=run
                            )
                            
                            if success:
                                successful_tests += 1
                                print(f"  ✓ Success")
                            else:
                                print(f"  ✗ Failed")
        
        # Phase 4: Save results
        print("\n" + "="*70)
        print("PHASE 4: SAVE RESULTS")
        print("="*70 + "\n")
        
        self.collector.save_results('results.json')
        self.collector.save_detailed_results()
        self.collector.save_summary()
        
        # Print summary
        print("\n" + "="*70)
        print("FINAL SUMMARY")
        print("="*70)
        print(f"Total tests: {total_tests}")
        print(f"Successful: {successful_tests}")
        print(f"Failed: {total_tests - successful_tests}")
        print(f"Success rate: {successful_tests/total_tests*100:.1f}%")
        print("="*70 + "\n")
        
        return successful_tests > 0
    
    def _run_operation(self,
                      build: str,
                      dataset_name: str,
                      dataset_path: Path,
                      operation: str,
                      format: str,
                      run_number: int) -> bool:
        """
        Run a single benchmark operation
        
        Returns:
            True if successful
        """
        if operation == 'create':
            return self._benchmark_create(build, dataset_name, dataset_path, format, run_number)
        elif operation == 'extract_full':
            return self._benchmark_extract_full(build, dataset_name, format, run_number)
        elif operation == 'extract_single':
            return self._benchmark_extract_single(build, dataset_name, format, run_number)
        else:
            print(f"  Unknown operation: {operation}")
            return False
    
    def _benchmark_create(self,
                         build: str,
                         dataset_name: str,
                         dataset_path: Path,
                         format: str,
                         run_number: int) -> bool:
        """Benchmark image creation"""
        tools = self.build_manager.get_all_tools(build)
        if 'mkdwarfs' not in tools:
            print(f"  ERROR: mkdwarfs not found")
            return False
        
        # Create temporary output
        with tempfile.TemporaryDirectory() as tmpdir:
            image_path = Path(tmpdir) / f"test_{format}.dwarfs"
            
            # Map format to --format option
            format_arg = 'flatbuffers' if format == 'flatbuffers' else 'thrift'
            
            cmd = [
                str(tools['mkdwarfs']),
                '-i', str(dataset_path),
                '-o', str(image_path),
                '--format', format_arg,
                '--compression', 'zstd:level=3',  # Fast for benchmarking
            ]
            
            # Measure time
            start = time.time()
            
            try:
                result = subprocess.run(
                    cmd,
                    capture_output=True,
                    text=True,
                    check=True
                )
                elapsed = time.time() - start
                
                # Get sizes
                input_size = sum(f.stat().st_size for f in dataset_path.rglob('*') if f.is_file())
                output_size = image_path.stat().st_size
                compression_ratio = input_size / output_size if output_size > 0 else 0
                
                # Collect metrics
                metrics = BenchmarkMetrics(
                    time_wall=elapsed,
                    size_input=input_size,
                    size_output=output_size,
                    compression_ratio=compression_ratio,
                )
                
                self.collector.collect_operation(
                    operation='create',
                    build_config=build,
                    dataset=dataset_name,
                    interface='cli',
                    format=format,
                    run_number=run_number,
                    metrics=metrics,
                    success=True
                )
                
                print(f"  Time: {elapsed:.3f}s, Size: {output_size/1024/1024:.2f} MB, Ratio: {compression_ratio:.2f}x")
                return True
                
            except subprocess.CalledProcessError as e:
                self.collector.collect_operation(
                    operation='create',
                    build_config=build,
                    dataset=dataset_name,
                    interface='cli',
                    format=format,
                    run_number=run_number,
                    metrics=BenchmarkMetrics(),
                    success=False,
                    error=str(e)
                )
                if self.verbose:
                    print(f"  ERROR: {e.stderr}")
                return False
    
    def _benchmark_extract_full(self,
                                build: str,
                                dataset_name: str,
                                format: str,
                                run_number: int) -> bool:
        """Benchmark full extraction"""
        tools = self.build_manager.get_all_tools(build)
        if 'dwarfsextract' not in tools:
            print(f"  ERROR: dwarfsextract not found")
            return False
        
        # Find or create test image
        image_path = self.dataset_manager.images_dir / f"{dataset_name}_{format}.dwarfs"
        
        if not image_path.exists():
            print(f"  Creating test image: {image_path}")
            dataset_path = self.dataset_manager.prepare_dataset(dataset_name)
            if not dataset_path:
                print(f"  ERROR: Dataset not available")
                return False
            
            images = self.dataset_manager.create_images(
                dataset_name,
                tools['mkdwarfs'],
                [format]
            )
            
            if format not in images:
                print(f"  ERROR: Failed to create {format} image")
                return False
            
            image_path = images[format]
        
        # Extract
        with tempfile.TemporaryDirectory() as tmpdir:
            extract_dir = Path(tmpdir) / "extracted"
            
            cmd = [
                str(tools['dwarfsextract']),
                '-i', str(image_path),
                '-o', str(extract_dir)
            ]
            
            start = time.time()
            
            try:
                result = subprocess.run(
                    cmd,
                    capture_output=True,
                    text=True,
                    check=True
                )
                elapsed = time.time() - start
                
                # Get extracted size
                extracted_size = sum(f.stat().st_size for f in extract_dir.rglob('*') if f.is_file())
                throughput = (extracted_size / 1024 / 1024) / elapsed if elapsed > 0 else 0
                
                metrics = BenchmarkMetrics(
                    time_wall=elapsed,
                    throughput_mb_s=throughput,
                    size_output=extracted_size,
                )
                
                self.collector.collect_operation(
                    operation='extract_full',
                    build_config=build,
                    dataset=dataset_name,
                    interface='cli',
                    format=format,
                    run_number=run_number,
                    metrics=metrics,
                    success=True
                )
                
                print(f"  Time: {elapsed:.3f}s, Throughput: {throughput:.2f} MB/s")
                return True
                
            except subprocess.CalledProcessError as e:
                self.collector.collect_operation(
                    operation='extract_full',
                    build_config=build,
                    dataset=dataset_name,
                    interface='cli',
                    format=format,
                    run_number=run_number,
                    metrics=BenchmarkMetrics(),
                    success=False,
                    error=str(e)
                )
                if self.verbose:
                    print(f"  ERROR: {e.stderr}")
                return False
    
    def _benchmark_extract_single(self,
                                  build: str,
                                  dataset_name: str,
                                  format: str,
                                  run_number: int) -> bool:
        """Benchmark single file extraction"""
        # Similar to extract_full but with --pattern
        # Simplified for now
        print(f"  SKIP: extract_single not yet implemented")
        return False


def main():
    parser = argparse.ArgumentParser(
        description='DwarFS Comprehensive Benchmark Suite',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Quick validation
  %(prog)s --builds fb-only,both --datasets dwarfs-source \\
           --operations create,extract_full --runs 3
  
  # Full suite
  %(prog)s --builds fb-only,both --datasets linux-kernel \\
           --operations all --runs 10
  
  # With regression check
  %(prog)s --builds both --datasets linux-kernel \\
           --operations all --regression-check --baseline v0.15.0
        """)
    
    parser.add_argument('--workspace', type=Path, default=Path.cwd(),
                       help='DwarFS workspace directory (default: current)')
    parser.add_argument('--builds', type=lambda s: s.split(','),
                       default=['fb-only', 'both'],
                       help='Build configurations (comma-separated, default: fb-only,both)')
    parser.add_argument('--datasets', type=lambda s: s.split(','),
                       default=['dwarfs-source'],
                       help='Datasets to test (comma-separated, default: dwarfs-source)')
    parser.add_argument('--operations', type=lambda s: s.split(',') if s != 'all' else ['create', 'extract_full'],
                       default=['create', 'extract_full'],
                       help='Operations to benchmark (comma-separated or "all", default: create,extract_full)')
    parser.add_argument('--formats', type=lambda s: s.split(','),
                       default=['flatbuffers', 'thrift'],
                       help='Formats to test (comma-separated, default: flatbuffers,thrift)')
    parser.add_argument('--runs', type=int, default=3,
                       help='Runs per test (default: 3)')
    parser.add_argument('--output-dir', type=Path,
                       default=Path('benchmark-results/comprehensive/latest'),
                       help='Output directory (default: benchmark-results/comprehensive/latest)')
    parser.add_argument('--regression-check', action='store_true',
                       help='Check for regressions against baseline')
    parser.add_argument('--baseline', type=Path,
                       help='Baseline results for regression check')
    parser.add_argument('--verbose', action='store_true',
                       help='Verbose output')
    
    args = parser.parse_args()
    
    # Create benchmark instance
    benchmark = ComprehensiveBenchmark(
        workspace=args.workspace,
        output_dir=args.output_dir,
        verbose=args.verbose
    )
    
    # Run suite
    success = benchmark.run_comprehensive_suite(
        builds=args.builds,
        datasets=args.datasets,
        operations=args.operations,
        runs_per_test=args.runs,
        formats=args.formats
    )
    
    if not success:
        print("\n✗ Benchmark suite FAILED")
        return 1
    
    # Regression check if requested
    if args.regression_check:
        if not args.baseline:
            print("\nERROR: --baseline required for --regression-check")
            return 1
        
        print("\n" + "="*70)
        print("REGRESSION CHECK")
        print("="*70 + "\n")
        
        comparison = benchmark.collector.compare_with_baseline(args.baseline)
        
        if comparison.get('has_regressions'):
            print("\n✗ REGRESSIONS DETECTED")
            return 1
    
    print("\n✓ Benchmark suite COMPLETE")
    return 0


if __name__ == '__main__':
    sys.exit(main())