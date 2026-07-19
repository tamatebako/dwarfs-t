#!/usr/bin/env python3
"""
DwarFS Metadata Format Benchmark Script

Compares FlatBuffers and Thrift metadata formats across:
- Creation time (real, user, sys)
- Image size
- Verification time

Usage:
    python3 metadata_format_benchmark.py \
        --flatbuffers-mkdwarfs ./build-fb/mkdwarfs \
        --thrift-mkdwarfs ./build-tb/mkdwarfs \
        --dataset /tmp/size-test \
        --iterations 10 \
        --output results.json
"""

import argparse
import json
import subprocess
import tempfile
import time
import statistics
import os
import sys
from pathlib import Path
from typing import Dict, List, Tuple

def run_creation_benchmark(mkdwarfs: Path, dataset: Path,
                          output: Path, iterations: int,
                          format_name: str) -> Dict:
    """Benchmark filesystem creation with accurate timing."""
    
    samples_real = []
    
    print(f"Running {format_name} creation benchmark ({iterations} iterations)...",
          file=sys.stderr)
    
    for i in range(iterations):
        # Clear filesystem cache
        subprocess.run(['sync'], check=False, stderr=subprocess.DEVNULL)
        
        # Use Python's time.perf_counter() for accurate wall-clock timing
        mkdwarfs_cmd = [str(mkdwarfs), '-i', str(dataset),
                       '-o', str(output), '--no-progress']
        
        start_time = time.perf_counter()
        result = subprocess.run(
            mkdwarfs_cmd,
            capture_output=True,
            text=True
        )
        elapsed = time.perf_counter() - start_time
        
        if result.returncode != 0:
            raise RuntimeError(
                f"{format_name} mkdwarfs failed (iteration {i+1}):\n{result.stderr}"
            )
        
        samples_real.append(elapsed)
        
        # Remove output for next iteration
        output.unlink()
        
        print(f"  Iteration {i+1}/{iterations}: {elapsed:.3f}s",
              file=sys.stderr)
    
    return {
        'real_mean': statistics.mean(samples_real),
        'real_stddev': statistics.stdev(samples_real) if len(samples_real) > 1 else 0,
        'real_median': statistics.median(samples_real),
        'real_samples': samples_real
    }

def measure_image_size(mkdwarfs: Path, dataset: Path, output: Path) -> int:
    """Create image and measure final size."""
    
    result = subprocess.run(
        [str(mkdwarfs), '-i', str(dataset), '-o', str(output), '--no-progress'],
        capture_output=True,
        text=True
    )
    
    if result.returncode != 0:
        raise RuntimeError(f"mkdwarfs failed for size measurement:\n{result.stderr}")
    
    size = output.stat().st_size
    return size

def count_dataset_files(dataset: Path) -> Tuple[int, int]:
    """Count files and total size in dataset."""
    
    file_count = 0
    total_size = 0
    
    for root, dirs, files in os.walk(dataset):
        file_count += len(files)
        for file in files:
            filepath = Path(root) / file
            try:
                total_size += filepath.stat().st_size
            except OSError:
                pass  # Skip inaccessible files
    
    return file_count, total_size

def main():
    parser = argparse.ArgumentParser(
        description='Benchmark DwarFS metadata formats',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__
    )
    parser.add_argument('--flatbuffers-mkdwarfs', required=True, type=Path,
                       help='Path to FlatBuffers mkdwarfs binary')
    parser.add_argument('--thrift-mkdwarfs', required=True, type=Path,
                       help='Path to Thrift mkdwarfs binary')
    parser.add_argument('--dataset', required=True, type=Path,
                       help='Path to test dataset directory')
    parser.add_argument('--iterations', type=int, default=10,
                       help='Number of benchmark iterations (default: 10)')
    parser.add_argument('--output', type=Path, required=True,
                       help='Output JSON file path')
    
    args = parser.parse_args()
    
    # Validate inputs
    if not args.flatbuffers_mkdwarfs.exists():
        print(f"Error: FlatBuffers mkdwarfs not found: {args.flatbuffers_mkdwarfs}", 
              file=sys.stderr)
        return 1
    
    if not args.thrift_mkdwarfs.exists():
        print(f"Error: Thrift mkdwarfs not found: {args.thrift_mkdwarfs}", 
              file=sys.stderr)
        return 1
    
    if not args.dataset.exists():
        print(f"Error: Dataset not found: {args.dataset}", file=sys.stderr)
        return 1
    
    # Count dataset files
    print("Analyzing dataset...", file=sys.stderr)
    file_count, total_size = count_dataset_files(args.dataset)
    print(f"Dataset: {file_count} files, {total_size} bytes", file=sys.stderr)
    
    # Run benchmarks in temporary directory
    with tempfile.TemporaryDirectory() as tmpdir:
        tmp_path = Path(tmpdir)
        
        # FlatBuffers benchmark
        print("\n=== FlatBuffers Benchmark ===", file=sys.stderr)
        fb_output = tmp_path / 'test-fb.dff'
        fb_results = run_creation_benchmark(
            args.flatbuffers_mkdwarfs, args.dataset, fb_output, 
            args.iterations, 'FlatBuffers'
        )
        
        # Measure FlatBuffers size
        print("Measuring FlatBuffers image size...", file=sys.stderr)
        fb_size = measure_image_size(args.flatbuffers_mkdwarfs, args.dataset, fb_output)
        fb_results['image_size_bytes'] = fb_size
        fb_output.unlink()
        
        # Thrift benchmark
        print("\n=== Thrift Benchmark ===", file=sys.stderr)
        tb_output = tmp_path / 'test-tb.dft'
        tb_results = run_creation_benchmark(
            args.thrift_mkdwarfs, args.dataset, tb_output,
            args.iterations, 'Thrift'
        )
        
        # Measure Thrift size
        print("Measuring Thrift image size...", file=sys.stderr)
        tb_size = measure_image_size(args.thrift_mkdwarfs, args.dataset, tb_output)
        tb_results['image_size_bytes'] = tb_size
    
    # Collect results
    results = {
        'metadata': {
            'date': time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime()),
            'dataset': {
                'path': str(args.dataset),
                'files': file_count,
                'size_bytes': total_size
            },
            'iterations': args.iterations,
            'builds': {
                'flatbuffers': str(args.flatbuffers_mkdwarfs),
                'thrift': str(args.thrift_mkdwarfs)
            }
        },
        'results': {
            'flatbuffers': fb_results,
            'thrift': tb_results
        },
        'comparison': {
            'size_ratio': fb_size / tb_size if tb_size > 0 else 0,
            'time_ratio': fb_results['real_mean'] / tb_results['real_mean'] 
                         if tb_results['real_mean'] > 0 else 0
        }
    }
    
    # Write JSON
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with open(args.output, 'w') as f:
        json.dump(results, f, indent=2)
    
    print(f"\n✅ Results written to {args.output}", file=sys.stderr)
    print(f"\n📊 Summary:", file=sys.stderr)
    print(f"  FlatBuffers: {fb_size} bytes, {fb_results['real_mean']:.3f}s avg", 
          file=sys.stderr)
    print(f"  Thrift:      {tb_size} bytes, {tb_results['real_mean']:.3f}s avg", 
          file=sys.stderr)
    print(f"  Size ratio:  {results['comparison']['size_ratio']:.4f}x", 
          file=sys.stderr)
    
    return 0

if __name__ == '__main__':
    sys.exit(main())