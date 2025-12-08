#!/usr/bin/env python3
"""
Unified Benchmark Runner for DwarFS

Orchestrates comprehensive benchmarks across:
- All 4 CLI tools (mkdwarfs, dwarfsck, dwarfsextract, dwarfs)
- Multiple datasets (tiny, perl, raspios)
- Both metadata formats (FlatBuffers, Thrift)
- All operations per tool

Generates a single unified JSON with all results.
"""

import argparse
import json
import os
import platform
import shutil
import subprocess
import sys
import tempfile
import time
from datetime import datetime
from pathlib import Path
from typing import Dict, List, Optional, Tuple

# Add lib to path for shared utilities
sys.path.insert(0, str(Path(__file__).parent))
from lib.memory_tracker import MemoryTracker
from lib.fuse_manager import FUSEManager
from lib.perfmon_parser import PerfmonParser
from lib.progress import ProgressDisplay


class BenchmarkRunner:
    """Orchestrates all DwarFS benchmarks."""
    
    def __init__(self, config: Dict):
        self.config = config
        self.results = {
            'metadata': {
                'date': datetime.utcnow().isoformat() + 'Z',
                'platform': platform.system(),
                'machine': platform.machine(),
                'tools': config['tools'],
                'datasets': list(config['datasets'].keys()),
                'iterations': config['iterations'],
                'compression_levels': config['levels']
            },
            'results': {}
        }
        self.memory_tracker = MemoryTracker()
        self.work_dir = Path(config['work_dir'])
        self.work_dir.mkdir(parents=True, exist_ok=True)
        self.image_dir = Path(config['image_dir'])
        self.image_dir.mkdir(parents=True, exist_ok=True)
        self.progress = ProgressDisplay()
        
    def run_all(self) -> Dict:
        """Main orchestrator - runs EVERYTHING."""
        print("=" * 80)
        print("DwarFS Unified Benchmark Suite")
        print("=" * 80)
        print()
        
        for dataset_name, dataset_path in self.config['datasets'].items():
            print(f"\n{'=' * 80}")
            print(f"Dataset: {dataset_name}")
            print(f"Path: {dataset_path}")
            print(f"{'=' * 80}\n")
            
            self.results['results'][dataset_name] = {}
            
            # 1. CREATE images (mkdwarfs)
            print(f"\n[1/4] Benchmarking mkdwarfs (creation)...")
            images = self.benchmark_mkdwarfs(dataset_name, dataset_path)
            
            # 2. VERIFY images (dwarfsck)
            print(f"\n[2/4] Benchmarking dwarfsck (verification)...")
            self.benchmark_dwarfsck(dataset_name, images)
            
            # 3. EXTRACT from images (dwarfsextract)
            print(f"\n[3/4] Benchmarking dwarfsextract (extraction)...")
            self.benchmark_dwarfsextract(dataset_name, images)
            
            # 4. MOUNT and ACCESS images (dwarfs)
            if self.config['tools']['flatbuffers'].get('dwarfs') or \
               self.config['tools']['thrift'].get('dwarfs'):
                print(f"\n[4/4] Benchmarking dwarfs (FUSE operations)...")
                self.benchmark_dwarfs_fuse(dataset_name, images)
            else:
                print(f"\n[4/4] Skipping dwarfs (FUSE not available)")
        
        return self.results
    
    def benchmark_mkdwarfs(self, dataset_name: str, dataset_path: str) -> Dict[str, str]:
        """Benchmark mkdwarfs with all formats and compression levels."""
        images = {}
        results = {'flatbuffers': {}, 'thrift': {}}
        
        for format_name in ['flatbuffers', 'thrift']:
            mkdwarfs = self.config['tools'][format_name].get('mkdwarfs')
            if not mkdwarfs:
                print(f"  Skipping {format_name} (mkdwarfs not available)")
                continue
            
            for level in self.config['levels']:
                level_key = f"l{level}"
                ext = 'dff' if format_name == 'flatbuffers' else 'dft'
                image_name = f"{dataset_name}-{format_name[:2]}-l{level}.{ext}"
                image_path = str(self.image_dir / image_name)
                
                print(f"\n  {format_name} level {level}:")
                
                # Run benchmark multiple times
                runs = []
                for run in range(self.config['iterations']):
                    # Remove old image
                    if os.path.exists(image_path):
                        os.remove(image_path)
                    
                    # Build command
                    cmd = [
                        mkdwarfs,
                        '-i', dataset_path,
                        '-o', image_path,
                        '-l', str(level),
                        '--log-level', 'error'
                    ]
                    
                    # Measure creation
                    print(f"    Run {run + 1}/{self.config['iterations']}...", end='', flush=True)
                    result = self.memory_tracker.measure_command(cmd)
                    
                    if result['exit_code'] == 0:
                        # Get image size
                        image_size = os.path.getsize(image_path)
                        dataset_size = self._get_directory_size(dataset_path)
                        
                        runs.append({
                            'wall_time': result['wall_time'],
                            'user_time': result['user_time'],
                            'sys_time': result['sys_time'],
                            'memory_mb': result['memory_mb'],
                            'image_size': image_size,
                            'dataset_size': dataset_size,
                            'compression_ratio': dataset_size / image_size if image_size > 0 else 0,
                            'throughput_mbps': (dataset_size / (1024 * 1024)) / result['wall_time'] if result['wall_time'] > 0 else 0
                        })
                        print(f" {result['wall_time']:.2f}s, {image_size:,} bytes")
                    else:
                        print(f" FAILED")
                        runs.append({'error': result.get('stderr', 'Unknown error')})
                
                # Store results
                results[format_name][level_key] = {
                    'runs': runs,
                    'image_path': image_path if os.path.exists(image_path) else None
                }
                
                # Save image path for later tests
                if os.path.exists(image_path):
                    images[f"{format_name}-{level_key}"] = image_path
        
        self.results['results'][dataset_name]['mkdwarfs'] = results
        return images
    
    def benchmark_dwarfsck(self, dataset_name: str, images: Dict[str, str]):
        """Benchmark dwarfsck with different operations."""
        results = {}
        
        for image_key, image_path in images.items():
            format_name = 'flatbuffers' if '-fb-' in image_key or image_key.startswith('flatbuffers') else 'thrift'
            dwarfsck = self.config['tools'][format_name].get('dwarfsck')
            if not dwarfsck:
                continue
            
            print(f"\n  {image_key}:")
            image_results = {}
            
            # 1. Quick check
            print(f"    Quick check...", end='', flush=True)
            quick_runs = []
            for _ in range(self.config['iterations']):
                result = self.memory_tracker.measure_command([
                    dwarfsck, '--check-integrity', image_path
                ])
                quick_runs.append({
                    'wall_time': result['wall_time'],
                    'exit_code': result['exit_code']
                })
            print(f" {quick_runs[0]['wall_time']:.2f}s avg")
            image_results['quick_check'] = {'runs': quick_runs}
            
            # 2. Full validation
            print(f"    Full validation...", end='', flush=True)
            full_runs = []
            for _ in range(self.config['iterations']):
                result = self.memory_tracker.measure_command([
                    dwarfsck, image_path
                ])
                full_runs.append({
                    'wall_time': result['wall_time'],
                    'exit_code': result['exit_code']
                })
            print(f" {full_runs[0]['wall_time']:.2f}s avg")
            image_results['full_validation'] = {'runs': full_runs}
            
            # 3. JSON export
            print(f"    JSON export...", end='', flush=True)
            json_runs = []
            for _ in range(self.config['iterations']):
                json_path = str(self.work_dir / f"{image_key}.json")
                result = self.memory_tracker.measure_command([
                    dwarfsck, '--json', '-o', json_path, image_path
                ])
                json_size = os.path.getsize(json_path) if os.path.exists(json_path) else 0
                json_runs.append({
                    'wall_time': result['wall_time'],
                    'json_size': json_size,
                    'exit_code': result['exit_code']
                })
                if os.path.exists(json_path):
                    os.remove(json_path)
            print(f" {json_runs[0]['wall_time']:.2f}s, {json_runs[0]['json_size']:,} bytes")
            image_results['json_export'] = {'runs': json_runs}
            
            results[image_key] = image_results
        
        self.results['results'][dataset_name]['dwarfsck'] = results
    
    def benchmark_dwarfsextract(self, dataset_name: str, images: Dict[str, str]):
        """Benchmark dwarfsextract with different operations."""
        results = {}
        
        for image_key, image_path in images.items():
            format_name = 'flatbuffers' if '-fb-' in image_key or image_key.startswith('flatbuffers') else 'thrift'
            dwarfsextract = self.config['tools'][format_name].get('dwarfsextract')
            if not dwarfsextract:
                continue
            
            print(f"\n  {image_key}:")
            image_results = {}
            
            # 1. Extract all files
            print(f"    Extract all...", end='', flush=True)
            extract_runs = []
            for _ in range(self.config['iterations']):
                extract_dir = self.work_dir / f"extract_{image_key}"
                if extract_dir.exists():
                    shutil.rmtree(extract_dir)
                
                result = self.memory_tracker.measure_command([
                    dwarfsextract, '-i', image_path, '-o', str(extract_dir)
                ])
                
                extracted_size = self._get_directory_size(str(extract_dir)) if extract_dir.exists() else 0
                extract_runs.append({
                    'wall_time': result['wall_time'],
                    'memory_mb': result['memory_mb'],
                    'extracted_size': extracted_size,
                    'throughput_mbps': (extracted_size / (1024 * 1024)) / result['wall_time'] if result['wall_time'] > 0 else 0,
                    'exit_code': result['exit_code']
                })
                
                if extract_dir.exists():
                    shutil.rmtree(extract_dir)
            
            print(f" {extract_runs[0]['wall_time']:.2f}s, {extract_runs[0]['throughput_mbps']:.1f} MB/s")
            image_results['extract_all'] = {'runs': extract_runs}
            
            # 2. Extract to tar
            print(f"    Convert to tar...", end='', flush=True)
            tar_runs = []
            for _ in range(self.config['iterations']):
                tar_path = str(self.work_dir / f"{image_key}.tar")
                if os.path.exists(tar_path):
                    os.remove(tar_path)
                
                result = self.memory_tracker.measure_command([
                    dwarfsextract, '-i', image_path, '-o', tar_path, '-f', 'ustar'
                ])
                
                tar_size = os.path.getsize(tar_path) if os.path.exists(tar_path) else 0
                tar_runs.append({
                    'wall_time': result['wall_time'],
                    'tar_size': tar_size,
                    'throughput_mbps': (tar_size / (1024 * 1024)) / result['wall_time'] if result['wall_time'] > 0 else 0,
                    'exit_code': result['exit_code']
                })
                
                if os.path.exists(tar_path):
                    os.remove(tar_path)
            
            print(f" {tar_runs[0]['wall_time']:.2f}s, {tar_runs[0]['throughput_mbps']:.1f} MB/s")
            image_results['to_tar'] = {'runs': tar_runs}
            
            results[image_key] = image_results
        
        self.results['results'][dataset_name]['dwarfsextract'] = results
    
    def benchmark_dwarfs_fuse(self, dataset_name: str, images: Dict[str, str]):
        """Benchmark dwarfs FUSE operations."""
        print("\n  Checking FUSE availability...", end='', flush=True)
        
        # Try FlatBuffers dwarfs first
        dwarfs_tool = self.config['tools']['flatbuffers'].get('dwarfs')
        if not dwarfs_tool and self.config['tools']['thrift'].get('dwarfs'):
            dwarfs_tool = self.config['tools']['thrift'].get('dwarfs')
        
        if not dwarfs_tool:
            print(" not available, skipping")
            return
        
        try:
            fuse_mgr = FUSEManager(dwarfs_tool)
            print(" available")
        except Exception as e:
            print(f" error: {e}, skipping")
            return
        
        results = {}
        
        for image_key, image_path in images.items():
            print(f"\n  {image_key}:")
            image_results = {}
            
            with tempfile.TemporaryDirectory() as mount_point:
                # Mount
                print(f"    Mounting...", end='', flush=True)
                mount_start = time.time()
                try:
                    proc = fuse_mgr.mount(image_path, mount_point, perfmon=True)
                    mount_time = time.time() - mount_start
                    print(f" {mount_time:.3f}s")
                    image_results['mount_time'] = mount_time
                except Exception as e:
                    print(f" failed: {e}")
                    continue
                
                # Sequential read (read first large file)
                print(f"    Sequential read...", end='', flush=True)
                try:
                    # Find a large file to read
                    for root, dirs, files in os.walk(mount_point):
                        if files:
                            test_file = os.path.join(root, files[0])
                            break
                    else:
                        test_file = None
                    
                    if test_file and os.path.exists(test_file):
                        start = time.time()
                        with open(test_file, 'rb') as f:
                            data = f.read()
                        read_time = time.time() - start
                        throughput = (len(data) / (1024 * 1024)) / read_time if read_time > 0 else 0
                        print(f" {read_time:.3f}s, {throughput:.1f} MB/s")
                        image_results['seq_read'] = {
                            'time': read_time,
                            'bytes': len(data),
                            'throughput_mbps': throughput
                        }
                    else:
                        print(" no files found")
                except Exception as e:
                    print(f" failed: {e}")
                
                # Directory traversal
                print(f"    Directory traversal...", end='', flush=True)
                try:
                    start = time.time()
                    file_count = sum(len(files) for _, _, files in os.walk(mount_point))
                    traversal_time = time.time() - start
                    print(f" {traversal_time:.3f}s, {file_count} files")
                    image_results['find'] = {
                        'time': traversal_time,
                        'file_count': file_count
                    }
                except Exception as e:
                    print(f" failed: {e}")
                
                # Get perfmon data
                try:
                    perfmon_text = fuse_mgr.get_perfmon_xattr(mount_point)
                    if perfmon_text:
                        metrics = PerfmonParser.parse(perfmon_text)
                        image_results['perfmon'] = metrics
                except Exception as e:
                    pass
                
                # Unmount
                print(f"    Unmounting...", end='', flush=True)
                try:
                    fuse_mgr.unmount(mount_point, proc)
                    print(" done")
                except Exception as e:
                    print(f" warning: {e}")
            
            results[image_key] = image_results
        
        self.results['results'][dataset_name]['dwarfs'] = results
    
    def _get_directory_size(self, path: str) -> int:
        """Get total size of directory."""
        total = 0
        if os.path.isfile(path):
            return os.path.getsize(path)
        for root, dirs, files in os.walk(path):
            for f in files:
                fp = os.path.join(root, f)
                if os.path.exists(fp):
                    total += os.path.getsize(fp)
        return total


def main():
    parser = argparse.ArgumentParser(
        description='DwarFS unified benchmark runner',
        formatter_class=argparse.RawDescriptionHelpFormatter
    )
    
    # Tool paths
    parser.add_argument('--flatbuffers-tools', required=True,
                        help='Path to FlatBuffers build directory')
    parser.add_argument('--thrift-tools', required=True,
                        help='Path to Thrift build directory')
    
    # Datasets
    parser.add_argument('--tiny-dataset', default='/tmp/size-test',
                        help='Path to tiny test dataset')
    parser.add_argument('--perl-dataset', 
                        help='Path to Perl dataset')
    parser.add_argument('--raspios-dataset',
                        help='Path to RaspOS dataset (optional)')
    
    # Options
    parser.add_argument('--levels', default='1,5,9',
                        help='Compression levels to test (comma-separated)')
    parser.add_argument('--iterations', type=int, default=3,
                        help='Number of iterations per test')
    parser.add_argument('--work-dir', default='/tmp/dwarfs_bench',
                        help='Working directory for temporary files')
    parser.add_argument('--image-dir', default='benchmark-results/images',
                        help='Directory to store benchmark images')
    parser.add_argument('--output', required=True,
                        help='Output JSON file')
    
    args = parser.parse_args()
    
    # Build tool configuration
    fb_dir = Path(args.flatbuffers_tools)
    tb_dir = Path(args.thrift_tools)
    
    config = {
        'tools': {
            'flatbuffers': {
                'mkdwarfs': str(fb_dir / 'mkdwarfs') if (fb_dir / 'mkdwarfs').exists() else None,
                'dwarfsck': str(fb_dir / 'dwarfsck') if (fb_dir / 'dwarfsck').exists() else None,
                'dwarfsextract': str(fb_dir / 'dwarfsextract') if (fb_dir / 'dwarfsextract').exists() else None,
                'dwarfs': str(fb_dir / 'dwarfs') if (fb_dir / 'dwarfs').exists() else None,
            },
            'thrift': {
                'mkdwarfs': str(tb_dir / 'mkdwarfs') if (tb_dir / 'mkdwarfs').exists() else None,
                'dwarfsck': str(tb_dir / 'dwarfsck') if (tb_dir / 'dwarfsck').exists() else None,
                'dwarfsextract': str(tb_dir / 'dwarfsextract') if (tb_dir / 'dwarfsextract').exists() else None,
                'dwarfs': str(tb_dir / 'dwarfs') if (tb_dir / 'dwarfs').exists() else None,
            }
        },
        'datasets': {},
        'levels': [int(x) for x in args.levels.split(',')],
        'iterations': args.iterations,
        'work_dir': args.work_dir,
        'image_dir': args.image_dir
    }
    
    # Add datasets
    if os.path.exists(args.tiny_dataset):
        config['datasets']['tiny'] = args.tiny_dataset
    if args.perl_dataset and os.path.exists(args.perl_dataset):
        config['datasets']['perl'] = args.perl_dataset
    if args.raspios_dataset and os.path.exists(args.raspios_dataset):
        config['datasets']['raspios'] = args.raspios_dataset
    
    if not config['datasets']:
        print("ERROR: No datasets found!", file=sys.stderr)
        return 1
    
    # Run benchmarks
    runner = BenchmarkRunner(config)
    results = runner.run_all()
    
    # Save results
    output_path = Path(args.output)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    with open(output_path, 'w') as f:
        json.dump(results, f, indent=2)
    
    print(f"\n{'=' * 80}")
    print(f"Results saved to: {output_path}")
    print(f"{'=' * 80}\n")
    
    return 0


if __name__ == '__main__':
    sys.exit(main())