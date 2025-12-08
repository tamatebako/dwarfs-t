#!/usr/bin/env python3
"""
DwarFS Extraction Performance Benchmark

Benchmarks dwarfsextract tool performance with various datasets.
Measures extraction time, throughput, memory usage, and CPU utilization.
"""

import argparse
import json
import os
import shutil
import subprocess
import sys
import tempfile
import time
from pathlib import Path
from typing import Dict, List, Optional

# Add lib directory to path
sys.path.insert(0, str(Path(__file__).parent / 'lib'))

from lib.benchmark_executor import BenchmarkExecutor
from lib.memory_tracker import MemoryTracker
from lib.result_formatter import ResultFormatter


class ExtractionBenchmark:
    """Benchmark extraction performance"""
    
    def __init__(self, dwarfsextract_path: str, output_dir: str):
        self.dwarfsextract = Path(dwarfsextract_path)
        self.output_dir = Path(output_dir)
        self.output_dir.mkdir(parents=True, exist_ok=True)
        self.results = []
        
    def benchmark_extraction(
        self,
        image_path: Path,
        dataset_name: str,
        runs: int = 3
    ) -> Dict:
        """Benchmark extraction of a single image"""
        
        print(f"\n{'='*70}")
        print(f"Benchmarking: {dataset_name}")
        print(f"Image: {image_path}")
        print(f"Runs: {runs}")
        print(f"{'='*70}\n")
        
        # Get image info
        image_size = image_path.stat().st_size
        print(f"Image size: {image_size:,} bytes ({image_size/1024/1024:.2f} MB)")
        
        run_results = []
        
        for run in range(1, runs + 1):
            print(f"\nRun {run}/{runs}...")
            
            # Create temporary extraction directory
            with tempfile.TemporaryDirectory() as tmpdir:
                extract_dir = Path(tmpdir) / "extracted"
                
                # Run extraction with time measurement
                start_time = time.time()
                
                cmd = [
                    str(self.dwarfsextract),
                    "-i", str(image_path),
                    "-o", str(extract_dir)
                ]
                
                try:
                    # Run with /usr/bin/time for detailed metrics (macOS)
                    time_cmd = ["/usr/bin/time", "-l"] + cmd
                    result = subprocess.run(
                        time_cmd,
                        capture_output=True,
                        text=True,
                        check=True
                    )
                    
                    end_time = time.time()
                    elapsed = end_time - start_time
                    
                    # Parse time output
                    metrics = self._parse_time_output(result.stderr)
                    metrics['elapsed_wall'] = elapsed
                    
                    # Get extracted size
                    extracted_size = self._get_directory_size(extract_dir)
                    metrics['extracted_size'] = extracted_size
                    metrics['image_size'] = image_size
                    metrics['compression_ratio'] = image_size / extracted_size if extracted_size > 0 else 0
                    
                    # Calculate throughput (MB/s of extracted data)
                    throughput = (extracted_size / 1024 / 1024) / elapsed if elapsed > 0 else 0
                    metrics['throughput_mb_s'] = throughput
                    
                    run_results.append(metrics)
                    
                    print(f"  Elapsed: {elapsed:.3f}s")
                    print(f"  Extracted: {extracted_size:,} bytes ({extracted_size/1024/1024:.2f} MB)")
                    print(f"  Throughput: {throughput:.2f} MB/s")
                    print(f"  Peak Memory: {metrics.get('max_rss', 0)/1024/1024:.2f} MB")
                    
                except subprocess.CalledProcessError as e:
                    print(f"  ERROR: Extraction failed: {e}")
                    print(f"  stdout: {e.stdout}")
                    print(f"  stderr: {e.stderr}")
                    continue
        
        if not run_results:
            print(f"\nERROR: No successful runs for {dataset_name}")
            return None
        
        # Calculate statistics
        result = {
            'dataset': dataset_name,
            'image_path': str(image_path),
            'image_size': image_size,
            'runs': runs,
            'successful_runs': len(run_results),
            'raw_results': run_results,
            'stats': self._calculate_stats(run_results)
        }
        
        self._print_summary(result)
        self.results.append(result)
        
        return result
    
    def _parse_time_output(self, stderr: str) -> Dict:
        """Parse /usr/bin/time -l output (macOS format)"""
        metrics = {}
        
        for line in stderr.split('\n'):
            line = line.strip()
            if not line:
                continue
                
            # Parse key metrics
            if 'real' in line and 'user' in line and 'sys' in line:
                # Format: "0.51 real         0.01 user         0.02 sys"
                parts = line.split()
                if len(parts) >= 6:
                    metrics['time_real'] = float(parts[0])
                    metrics['time_user'] = float(parts[2])
                    metrics['time_sys'] = float(parts[4])
            elif 'maximum resident set size' in line:
                metrics['max_rss'] = int(line.split()[0])
            elif 'page reclaims' in line:
                metrics['page_reclaims'] = int(line.split()[0])
            elif 'page faults' in line:
                metrics['page_faults'] = int(line.split()[0])
            elif 'voluntary context switches' in line:
                metrics['voluntary_ctx_switches'] = int(line.split()[0])
            elif 'involuntary context switches' in line:
                metrics['involuntary_ctx_switches'] = int(line.split()[0])
            elif 'instructions retired' in line:
                metrics['instructions'] = int(line.split()[0])
            elif 'cycles elapsed' in line:
                metrics['cycles'] = int(line.split()[0])
            elif 'peak memory footprint' in line:
                metrics['peak_footprint'] = int(line.split()[0])
        
        return metrics
    
    def _get_directory_size(self, path: Path) -> int:
        """Calculate total size of directory"""
        total = 0
        for item in path.rglob('*'):
            if item.is_file():
                total += item.stat().st_size
        return total
    
    def _calculate_stats(self, run_results: List[Dict]) -> Dict:
        """Calculate statistics from multiple runs"""
        import statistics
        
        def get_values(key: str) -> List[float]:
            return [r.get(key, 0) for r in run_results if key in r]
        
        def calc_stat(key: str) -> Dict:
            values = get_values(key)
            if not values:
                return {}
            return {
                'mean': statistics.mean(values),
                'median': statistics.median(values),
                'stdev': statistics.stdev(values) if len(values) > 1 else 0,
                'min': min(values),
                'max': max(values)
            }
        
        return {
            'elapsed_wall': calc_stat('elapsed_wall'),
            'throughput_mb_s': calc_stat('throughput_mb_s'),
            'max_rss': calc_stat('max_rss'),
            'time_user': calc_stat('time_user'),
            'time_sys': calc_stat('time_sys'),
            'page_faults': calc_stat('page_faults'),
            'instructions': calc_stat('instructions'),
            'cycles': calc_stat('cycles'),
        }
    
    def _print_summary(self, result: Dict):
        """Print summary statistics"""
        stats = result['stats']
        
        print(f"\n{'-'*70}")
        print(f"Summary for {result['dataset']}:")
        print(f"{'-'*70}")
        print(f"Successful runs: {result['successful_runs']}/{result['runs']}")
        
        if 'elapsed_wall' in stats and stats['elapsed_wall']:
            elapsed = stats['elapsed_wall']
            print(f"\nExtraction Time:")
            print(f"  Mean:   {elapsed['mean']:.3f}s")
            print(f"  Median: {elapsed['median']:.3f}s")
            print(f"  StdDev: {elapsed['stdev']:.3f}s")
            print(f"  Range:  {elapsed['min']:.3f}s - {elapsed['max']:.3f}s")
        
        if 'throughput_mb_s' in stats and stats['throughput_mb_s']:
            throughput = stats['throughput_mb_s']
            print(f"\nThroughput:")
            print(f"  Mean:   {throughput['mean']:.2f} MB/s")
            print(f"  Median: {throughput['median']:.2f} MB/s")
            print(f"  Range:  {throughput['min']:.2f} - {throughput['max']:.2f} MB/s")
        
        if 'max_rss' in stats and stats['max_rss']:
            mem = stats['max_rss']
            print(f"\nPeak Memory (RSS):")
            print(f"  Mean:   {mem['mean']/1024/1024:.2f} MB")
            print(f"  Median: {mem['median']/1024/1024:.2f} MB")
            print(f"  Range:  {mem['min']/1024/1024:.2f} - {mem['max']/1024/1024:.2f} MB")
        
        print(f"{'-'*70}\n")
    
    def save_results(self, output_file: str):
        """Save results to JSON file"""
        output_path = self.output_dir / output_file
        with open(output_path, 'w') as f:
            json.dump(self.results, f, indent=2)
        print(f"\nResults saved to: {output_path}")
        return output_path


def main():
    parser = argparse.ArgumentParser(description='Benchmark DwarFS extraction performance')
    parser.add_argument('--dwarfsextract', default='./build-fb/dwarfsextract',
                        help='Path to dwarfsextract binary')
    parser.add_argument('--output-dir', default='benchmark-results/extraction',
                        help='Output directory for results')
    parser.add_argument('--runs', type=int, default=3,
                        help='Number of runs per benchmark')
    parser.add_argument('--images', nargs='+',
                        help='Specific image files to benchmark (overrides auto-discovery)')
    
    args = parser.parse_args()
    
    # Discover or use specified images
    if args.images:
        image_paths = [Path(img) for img in args.images]
        dataset_names = [img.stem for img in image_paths]
    else:
        # Auto-discover images
        image_paths = []
        dataset_names = []
        
        search_dirs = [
            Path('benchmark-results'),
            Path('benchmark_data'),
        ]
        
        for search_dir in search_dirs:
            if search_dir.exists():
                for ext in ['*.dwarfs', '*.dff']:
                    for img in search_dir.rglob(ext):
                        if img.is_file():
                            image_paths.append(img)
                            # Create readable name from path
                            rel_path = img.relative_to(search_dir) if img.is_relative_to(search_dir) else img
                            dataset_names.append(str(rel_path.with_suffix('')).replace('/', '-'))
    
    if not image_paths:
        print("ERROR: No DwarFS images found!")
        print("Please create images or specify paths with --images")
        return 1
    
    print(f"Found {len(image_paths)} image(s) to benchmark")
    
    # Run benchmarks
    benchmark = ExtractionBenchmark(
        dwarfsextract_path=args.dwarfsextract,
        output_dir=args.output_dir
    )
    
    for img_path, dataset_name in zip(image_paths, dataset_names):
        if not img_path.exists():
            print(f"WARNING: Image not found: {img_path}")
            continue
            
        benchmark.benchmark_extraction(
            image_path=img_path,
            dataset_name=dataset_name,
            runs=args.runs
        )
    
    # Save results
    if benchmark.results:
        output_file = benchmark.save_results('extraction_benchmark_results.json')
        
        # Print overall summary
        print("\n" + "="*70)
        print("OVERALL SUMMARY")
        print("="*70)
        for result in benchmark.results:
            stats = result['stats']
            elapsed = stats.get('elapsed_wall', {})
            throughput = stats.get('throughput_mb_s', {})
            memory = stats.get('max_rss', {})
            
            print(f"\n{result['dataset']}:")
            if elapsed:
                print(f"  Time: {elapsed.get('median', 0):.3f}s (median)")
            if throughput:
                print(f"  Throughput: {throughput.get('median', 0):.2f} MB/s (median)")
            if memory:
                print(f"  Memory: {memory.get('median', 0)/1024/1024:.2f} MB (median)")
        
        print(f"\nFull results: {output_file}")
        return 0
    else:
        print("\nERROR: No successful benchmarks")
        return 1


if __name__ == '__main__':
    sys.exit(main())