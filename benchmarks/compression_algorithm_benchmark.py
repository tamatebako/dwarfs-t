#!/usr/bin/env python3
"""
DwarFS Compression Algorithm Benchmark Runner

Automates running compression algorithm benchmarks across build configurations.
Parses Google Test output and generates structured results.
"""

import argparse
import json
import re
import subprocess
import sys
import time
from dataclasses import dataclass, asdict, field
from pathlib import Path
from typing import Dict, List, Optional, Any

# Import existing benchmark infrastructure
try:
    from lib.memory_tracker import MemoryTracker
except ImportError:
    # Fallback if lib not in path
    sys.path.insert(0, str(Path(__file__).parent))
    from lib.memory_tracker import MemoryTracker


@dataclass
class AlgorithmResult:
    """Result from a single algorithm benchmark test"""
    algorithm: str
    level_or_option: str
    dataset: str
    
    # Sizes
    input_size_bytes: int = 0
    output_size_bytes: int = 0
    compression_ratio_percent: float = 0.0
    
    # Timing
    compression_time_ms: float = 0.0
    decompression_time_ms: float = 0.0
    
    # Throughput
    compression_speed_mbps: float = 0.0
    decompression_speed_mbps: float = 0.0
    
    # Verification
    decompression_successful: bool = False
    data_matches: bool = False
    
    # Test metadata
    test_name: str = ""
    test_passed: bool = False
    error_message: str = ""


@dataclass
class BenchmarkConfig:
    """Configuration for benchmark execution"""
    build_dir: Path
    algorithms: Optional[List[str]] = None  # None = all
    output_file: Path = Path("benchmark-results/compression-algorithms.json")
    verbose: bool = False
    use_memory_tracking: bool = True


@dataclass
class BenchmarkSummary:
    """Summary of all benchmark results"""
    timestamp: float
    build_dir: str
    build_config: Dict[str, Any] = field(default_factory=dict)
    test_results: List[AlgorithmResult] = field(default_factory=list)
    total_tests: int = 0
    passed_tests: int = 0
    failed_tests: int = 0
    execution_time_seconds: float = 0.0


class CompressionBenchmarkRunner:
    """
    Single Responsibility: Automate compression algorithm benchmarking
    
    Runs benchmark executable, parses output, generates reports
    """
    
    def __init__(self, config: BenchmarkConfig):
        self.config = config
        self.benchmark_exe = config.build_dir / "dwarfs_compression_benchmark"
        self.memory_tracker = MemoryTracker() if config.use_memory_tracking else None
        
        # Validate benchmark executable exists
        if not self.benchmark_exe.exists():
            raise FileNotFoundError(
                f"Benchmark executable not found: {self.benchmark_exe}\n"
                f"Build it with: cmake --build {config.build_dir} "
                f"--target dwarfs_compression_benchmark"
            )
    
    def detect_build_config(self) -> Dict[str, Any]:
        """Detect build configuration (FlatBuffers, Thrift, etc.)"""
        config = {
            'flatbuffers': False,
            'thrift': False,
            'build_type': 'unknown',
        }
        
        # Check CMakeCache.txt
        cache_file = self.config.build_dir / "CMakeCache.txt"
        if cache_file.exists():
            content = cache_file.read_text()
            
            if 'DWARFS_WITH_FLATBUFFERS:BOOL=ON' in content:
                config['flatbuffers'] = True
            if 'DWARFS_WITH_THRIFT:BOOL=ON' in content:
                config['thrift'] = True
            
            # Detect build type
            match = re.search(r'CMAKE_BUILD_TYPE:STRING=(\w+)', content)
            if match:
                config['build_type'] = match.group(1)
        
        return config
    
    def run_all_tests(self) -> BenchmarkSummary:
        """Run all benchmark tests"""
        if self.config.verbose:
            print(f"Running benchmarks from: {self.benchmark_exe}")
        
        start_time = time.time()
        
        # Build filter if specific algorithms requested
        gtest_filter = None
        if self.config.algorithms:
            # Convert algorithm names to test filter pattern
            filter_patterns = [f"*{alg}*" for alg in self.config.algorithms]
            gtest_filter = ":".join(filter_patterns)
        
        # Run benchmark with Google Test
        results = self._run_benchmark_executable(gtest_filter)
        
        # Detect build configuration
        build_config = self.detect_build_config()
        
        # Parse results
        test_results = self._parse_test_output(results['stdout'])
        
        # Create summary
        summary = BenchmarkSummary(
            timestamp=time.time(),
            build_dir=str(self.config.build_dir),
            build_config=build_config,
            test_results=test_results,
            total_tests=len(test_results),
            passed_tests=sum(1 for r in test_results if r.test_passed),
            failed_tests=sum(1 for r in test_results if not r.test_passed),
            execution_time_seconds=time.time() - start_time
        )
        
        return summary
    
    def _run_benchmark_executable(self, gtest_filter: Optional[str] = None) -> Dict[str, Any]:
        """Execute benchmark binary and capture output"""
        cmd = [str(self.benchmark_exe)]
        
        # Add filter if specified
        if gtest_filter:
            cmd.append(f"--gtest_filter={gtest_filter}")
        
        if self.config.verbose:
            print(f"Executing: {' '.join(cmd)}")
        
        # Run with or without memory tracking
        if self.memory_tracker:
            result = self.memory_tracker.measure_command(cmd)
            stdout = result['stdout']
            stderr = result['stderr']
            returncode = result['returncode']
            
            if self.config.verbose:
                print(f"Memory: {result['memory_mb']:.1f} MB")
                print(f"Time: {result['wall_time']:.1f}s")
        else:
            process = subprocess.run(
                cmd,
                capture_output=True,
                text=True
            )
            stdout = process.stdout
            stderr = process.stderr
            returncode = process.returncode
        
        if returncode != 0 and self.config.verbose:
            print(f"WARNING: Benchmark returned non-zero: {returncode}")
            if stderr:
                print(f"STDERR: {stderr}")
        
        return {
            'stdout': stdout,
            'stderr': stderr,
            'returncode': returncode
        }
    
    def _parse_test_output(self, output: str) -> List[AlgorithmResult]:
        """Parse Google Test output to extract benchmark results"""
        results = []
        
        # Split into individual test outputs
        test_pattern = re.compile(
            r'\[ RUN      \] (CompressionAlgorithmBenchmark\.\w+).*?'
            r'\[       OK \]|'
            r'\[  FAILED  \]',
            re.DOTALL
        )
        
        # Extract algorithm results from test output
        algorithm_pattern = re.compile(
            r'Algorithm: (\w+):(\S+)\s+'
            r'Dataset: (\w+)\s+'
            r'Input: (\d+) bytes\s+'
            r'Output: (\d+) bytes\s+'
            r'Ratio: ([\d.]+)%\s+'
            r'Compression: ([\d.]+) ms \(([\d.]+) MB/s\)\s+'
            r'Decompression: ([\d.]+) ms \(([\d.]+) MB/s\)\s+'
            r'Verification: (\w+)'
        )
        
        # Find all algorithm results
        for match in algorithm_pattern.finditer(output):
            result = AlgorithmResult(
                algorithm=match.group(1),
                level_or_option=match.group(2),
                dataset=match.group(3),
                input_size_bytes=int(match.group(4)),
                output_size_bytes=int(match.group(5)),
                compression_ratio_percent=float(match.group(6)),
                compression_time_ms=float(match.group(7)),
                compression_speed_mbps=float(match.group(8)),
                decompression_time_ms=float(match.group(9)),
                decompression_speed_mbps=float(match.group(10)),
                decompression_successful=True,
                data_matches=(match.group(11) == "PASSED"),
                test_passed=(match.group(11) == "PASSED")
            )
            results.append(result)
        
        # Extract test pass/fail status
        test_status_pattern = re.compile(
            r'\[ RUN      \] (CompressionAlgorithmBenchmark\.\w+).*?'
            r'\[       OK \] \1|'
            r'\[  FAILED  \] \1',
            re.DOTALL
        )
        
        return results
    
    def save_results(self, summary: BenchmarkSummary) -> None:
        """Save results to JSON file"""
        output_file = self.config.output_file
        output_file.parent.mkdir(parents=True, exist_ok=True)
        
        # Convert to dict for JSON serialization
        data = asdict(summary)
        
        with open(output_file, 'w') as f:
            json.dump(data, f, indent=2)
        
        print(f"\nResults saved to: {output_file}")
    
    def print_summary(self, summary: BenchmarkSummary) -> None:
        """Print human-readable summary"""
        print("\n" + "="*70)
        print("Compression Algorithm Benchmark Summary")
        print("="*70)
        
        print(f"\nBuild Configuration:")
        print(f"  Directory: {summary.build_dir}")
        print(f"  FlatBuffers: {'✅' if summary.build_config.get('flatbuffers') else '❌'}")
        print(f"  Thrift: {'✅' if summary.build_config.get('thrift') else '❌'}")
        print(f"  Build Type: {summary.build_config.get('build_type', 'unknown')}")
        
        print(f"\nTest Results:")
        print(f"  Total: {summary.total_tests}")
        print(f"  Passed: {summary.passed_tests}")
        print(f"  Failed: {summary.failed_tests}")
        print(f"  Execution Time: {summary.execution_time_seconds:.1f}s")
        
        if summary.test_results:
            print(f"\nAlgorithm Performance:")
            
            # Group by algorithm
            by_algorithm = {}
            for result in summary.test_results:
                alg = result.algorithm
                if alg not in by_algorithm:
                    by_algorithm[alg] = []
                by_algorithm[alg].append(result)
            
            for alg, results in sorted(by_algorithm.items()):
                print(f"\n  {alg.upper()}:")
                for r in results:
                    status = "✅" if r.test_passed else "❌"
                    print(f"    {status} {r.level_or_option:20s} "
                          f"Ratio: {r.compression_ratio_percent:6.2f}% "
                          f"Speed: {r.compression_speed_mbps:7.1f} MB/s")
        
        print("\n" + "="*70)


def main():
    parser = argparse.ArgumentParser(
        description='DwarFS Compression Algorithm Benchmark Runner',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Run all benchmarks on FlatBuffers-only build
  %(prog)s --build-dir build-fb
  
  # Run specific algorithms only
  %(prog)s --build-dir build-fb --algorithms zstd lzma
  
  # Run with verbose output
  %(prog)s --build-dir build-fb --verbose
  
  # Custom output location
  %(prog)s --build-dir build-fb --output results/my-benchmark.json
        """
    )
    
    parser.add_argument(
        '--build-dir',
        type=Path,
        required=True,
        help='Path to build directory containing dwarfs_compression_benchmark'
    )
    
    parser.add_argument(
        '--algorithms',
        nargs='+',
        help='Specific algorithms to test (e.g., zstd lzma flac). Default: all'
    )
    
    parser.add_argument(
        '--output',
        type=Path,
        default=Path('benchmark-results/compression-algorithms.json'),
        help='Output JSON file (default: benchmark-results/compression-algorithms.json)'
    )
    
    parser.add_argument(
        '--verbose',
        action='store_true',
        help='Enable verbose output'
    )
    
    parser.add_argument(
        '--no-memory-tracking',
        action='store_true',
        help='Disable memory usage tracking'
    )
    
    args = parser.parse_args()
    
    # Create configuration
    config = BenchmarkConfig(
        build_dir=args.build_dir,
        algorithms=args.algorithms,
        output_file=args.output,
        verbose=args.verbose,
        use_memory_tracking=not args.no_memory_tracking
    )
    
    # Run benchmarks
    try:
        runner = CompressionBenchmarkRunner(config)
        summary = runner.run_all_tests()
        
        # Save and display results
        runner.save_results(summary)
        runner.print_summary(summary)
        
        # Exit with success if all tests passed
        return 0 if summary.failed_tests == 0 else 1
        
    except FileNotFoundError as e:
        print(f"ERROR: {e}", file=sys.stderr)
        return 1
    except Exception as e:
        print(f"ERROR: Unexpected error: {e}", file=sys.stderr)
        if args.verbose:
            import traceback
            traceback.print_exc()
        return 1


if __name__ == '__main__':
    sys.exit(main())