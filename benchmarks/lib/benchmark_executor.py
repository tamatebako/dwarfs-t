#!/usr/bin/env python3
"""
DwarFS Format Benchmark Executor

Single Responsibility: Execute benchmarks and collect metrics
Configuration-driven from YAML files
Object-oriented with clean interfaces
"""

import yaml
import json
import time
import subprocess
import os
import shutil
import random
import tempfile
from dataclasses import dataclass, asdict, field
from typing import List, Dict, Any, Optional
from pathlib import Path
from abc import ABC, abstractmethod


@dataclass
class MetricResult:
    """Value object for a single metric measurement"""
    name: str
    value: float
    unit: str
    timestamp: float
    metadata: Dict[str, Any] = field(default_factory=dict)


@dataclass
class BenchmarkResult:
    """Value object for complete benchmark of one format"""
    format_name: str
    dataset_name: str
    metrics: List[MetricResult]
    image_path: str
    success: bool
    error_message: str = ""


class IMetricCollector(ABC):
    """
    Interface for metric collection

    Single Responsibility: Define metric collection contract
    Open/Closed: New collectors without modifying existing
    """

    @abstractmethod
    def collect(self, context: Dict[str, Any]) -> Optional[MetricResult]:
        """Collect a metric measurement"""
        pass

    @abstractmethod
    def get_name(self) -> str:
        """Get metric name"""
        pass


class BuildTimeCollector(IMetricCollector):
    """Single Responsibility: Measure filesystem creation time"""

    def get_name(self) -> str:
        return "build_time"

    def collect(self, context: Dict[str, Any]) -> Optional[MetricResult]:
        create_cmd = context['create_cmd']

        print(f"  Measuring build time...")
        start = time.time()

        try:
            result = subprocess.run(
                create_cmd,
                shell=True,
                capture_output=True,
                text=True
            )

            if result.returncode != 0:
                print(f"    ERROR: {result.stderr}")
                return None

            elapsed = time.time() - start

            return MetricResult(
                name="build_time",
                value=elapsed,
                unit="seconds",
                timestamp=time.time(),
                metadata={'returncode': result.returncode}
            )

        except Exception as e:
            print(f"    ERROR: {e}")
            return None


class ImageSizeCollector(IMetricCollector):
    """Single Responsibility: Measure created image size"""

    def get_name(self) -> str:
        return "image_size"

    def collect(self, context: Dict[str, Any]) -> Optional[MetricResult]:
        image_path = context['image_path']

        print(f"  Measuring image size...")

        if not os.path.exists(image_path):
            print(f"    ERROR: Image not found: {image_path}")
            return None

        size = os.path.getsize(image_path)

        return MetricResult(
            name="image_size",
            value=size,
            unit="bytes",
            timestamp=time.time()
        )


class CompressionRatioCollector(IMetricCollector):
    """Single Responsibility: Calculate compression ratio"""

    def get_name(self) -> str:
        return "compression_ratio"

    def collect(self, context: Dict[str, Any]) -> Optional[MetricResult]:
        source_size = context.get('source_size', 0)
        image_size_result = context.get('image_size_result')

        if not image_size_result or source_size == 0:
            return None

        print(f"  Calculating compression ratio...")

        image_size = image_size_result.value
        ratio = source_size / image_size if image_size > 0 else 0

        return MetricResult(
            name="compression_ratio",
            value=ratio,
            unit="ratio",
            timestamp=time.time(),
            metadata={
                'source_size': source_size,
                'image_size': image_size
            }
        )


class ExtractionTimeCollector(IMetricCollector):
    """Single Responsibility: Measure full extraction time"""

    def get_name(self) -> str:
        return "full_extract_time"

    def collect(self, context: Dict[str, Any]) -> Optional[MetricResult]:
        extract_cmd = context['extract_cmd']
        extract_dir = context['extract_dir']

        print(f"  Measuring extraction time...")

        # Create extract directory
        Path(extract_dir).mkdir(parents=True, exist_ok=True)

        start = time.time()

        try:
            result = subprocess.run(
                extract_cmd,
                shell=True,
                capture_output=True,
                text=True
            )

            if result.returncode != 0:
                print(f"    ERROR: {result.stderr}")
                return None

            elapsed = time.time() - start

            return MetricResult(
                name="full_extract_time",
                value=elapsed,
                unit="seconds",
                timestamp=time.time()
            )

        except Exception as e:
            print(f"    ERROR: {e}")
            return None


class FUSERandomAccessCollector(IMetricCollector):
    """
    Simple FUSE random access testing

    Mounts filesystem, reads random files, measures latency
    """

    def get_name(self) -> str:
        return "fuse_random_access"

    def collect(self, context: Dict[str, Any]) -> Optional[MetricResult]:
        image_path = context['image_path']
        tools = context['tools']

        # Check if dwarfs executable exists
        dwarfs_path = tools.get('dwarfs')
        if not dwarfs_path or not Path(dwarfs_path).exists():
            print("    SKIP: dwarfs mount tool not available")
            return None

        print(f"  Testing FUSE random access...")

        # Create temporary mount point
        with tempfile.TemporaryDirectory() as mount_point:
            # Mount filesystem
            mount_cmd = f"{dwarfs_path} {image_path} {mount_point}"

            mount_proc = None
            try:
                # Mount in background
                mount_proc = subprocess.Popen(
                    mount_cmd.split(),
                    stdout=subprocess.DEVNULL,
                    stderr=subprocess.DEVNULL
                )

                # Wait a bit for mount to initialize
                time.sleep(2)

                # Find some files to test
                files = []
                for root, dirs, filenames in os.walk(mount_point):
                    for f in filenames[:50]:  # Sample 50 files
                        files.append(Path(root) / f)
                    if len(files) >= 50:
                        break

                if not files:
                    print("    SKIP: No files found in mounted filesystem")
                    return None

                # Random access test: read 20 random files
                latencies = []
                for _ in range(20):
                    file = random.choice(files)

                    start = time.time()
                    try:
                        with open(file, 'rb') as f:
                            f.read(4096)  # Read first 4KB
                    except:
                        continue

                    latency_ms = (time.time() - start) * 1000
                    latencies.append(latency_ms)

                # Calculate median (p50)
                if latencies:
                    latencies.sort()
                    p50 = latencies[len(latencies) // 2]
                    p99 = latencies[int(len(latencies) * 0.99)] if len(latencies) > 10 else latencies[-1]

                    return MetricResult(
                        name="fuse_random_access_p50",
                        value=p50,
                        unit="milliseconds",
                        timestamp=time.time(),
                        metadata={
                            'p99': p99,
                            'samples': len(latencies)
                        }
                    )

                return None

            except Exception as e:
                print(f"    ERROR: {e}")
                return None

            finally:
                # Unmount
                if mount_proc:
                    mount_proc.terminate()
                    mount_proc.wait(timeout=5)

                # Try unmount (works on both Linux and macOS with fuse-t)
                subprocess.run(
                    ['umount', mount_point],
                    capture_output=True
                )

        return None


class FUSEFileCountCollector(IMetricCollector):
    """
    Simple test: How many files in filesystem?

    Via FUSE mount - tests metadata traversal
    """

    def get_name(self) -> str:
        return "fuse_file_count"

    def collect(self, context: Dict[str, Any]) -> Optional[MetricResult]:
        image_path = context['image_path']
        tools = context['tools']

        dwarfs_path = tools.get('dwarfs')
        if not dwarfs_path:
            return None

        print(f"  Counting files via FUSE mount...")

        mount_proc = None
        with tempfile.TemporaryDirectory() as mount_point:
            mount_cmd = f"{dwarfs_path} {image_path} {mount_point}"

            try:
                mount_proc = subprocess.Popen(
                    mount_cmd.split(),
                    stdout=subprocess.DEVNULL,
                    stderr=subprocess.DEVNULL
                )

                time.sleep(2)

                # Count files
                start = time.time()
                file_count = 0

                for root, dirs, files in os.walk(mount_point):
                    file_count += len(files)

                count_time = time.time() - start

                return MetricResult(
                    name="fuse_file_count",
                    value=file_count,
                    unit="files",
                    timestamp=time.time(),
                    metadata={'count_time_seconds': count_time}
                )

            except Exception as e:
                print(f"    ERROR: {e}")
                return None

            finally:
                if mount_proc:
                    mount_proc.terminate()
                    mount_proc.wait(timeout=5)
                subprocess.run(['umount', mount_point],
                             capture_output=True)

        return None


class FUSELargeFileAccessCollector(IMetricCollector):
    """
    Single Responsibility: Measure random access on large files

    Tests block decompression efficiency
    """

    def get_name(self) -> str:
        return "fuse_large_file_access"

    def collect(self, context: Dict[str, Any]) -> Optional[MetricResult]:
        image_path = context['image_path']
        tools = context.get('tools', {})
        dwarfs_path = tools.get('dwarfs')

        if not dwarfs_path or not Path(dwarfs_path).exists():
            print("    SKIP: dwarfs not available")
            return None

        print(f"  Testing large file random access...")

        with tempfile.TemporaryDirectory() as mount_point:
            mount_cmd = f"{dwarfs_path} {image_path} {mount_point}"

            mount_proc = None
            try:
                mount_proc = subprocess.Popen(
                    mount_cmd.split(),
                    stdout=subprocess.DEVNULL,
                    stderr=subprocess.DEVNULL
                )
                time.sleep(2)

                # Find large files (>1MB)
                large_files = []
                for root, dirs, files in os.walk(mount_point):
                    for f in files:
                        path = Path(root) / f
                        try:
                            if path.stat().st_size > 1024 * 1024:  # >1MB
                                large_files.append(path)
                                if len(large_files) >= 5:
                                    break
                        except:
                            continue
                    if len(large_files) >= 5:
                        break

                if not large_files:
                    print("    SKIP: No large files found")
                    return None

                # Random seeks + reads (64KB chunks)
                latencies = []
                for file in large_files:
                    file_size = file.stat().st_size

                    # 10 random seeks per file
                    for _ in range(10):
                        offset = random.randint(0, max(0, file_size - 65536))

                        start = time.time()
                        try:
                            with open(file, 'rb') as f:
                                f.seek(offset)
                                f.read(65536)  # 64KB
                        except:
                            continue

                        latency_ms = (time.time() - start) * 1000
                        latencies.append(latency_ms)

                if latencies:
                    latencies.sort()
                    p50 = latencies[len(latencies) // 2]
                    p90 = latencies[int(len(latencies) * 0.9)]
                    p99 = latencies[int(len(latencies) * 0.99)] if len(latencies) > 10 else latencies[-1]

                    return MetricResult(
                        name="fuse_large_file_p50",
                        value=p50,
                        unit="milliseconds",
                        timestamp=time.time(),
                        metadata={
                            'p90': p90,
                            'p99': p99,
                            'samples': len(latencies),
                            'files_tested': len(large_files)
                        }
                    )

                return None

            except Exception as e:
                print(f"    ERROR: {e}")
                return None
            finally:
                if mount_proc:
                    mount_proc.terminate()
                    mount_proc.wait(timeout=5)
                subprocess.run(['umount', mount_point],
                             capture_output=True)

        return None


class FUSESmallFileAccessCollector(IMetricCollector):
    """
    Single Responsibility: Measure access to many small files

    Tests metadata lookup performance
    """

    def get_name(self) -> str:
        return "fuse_small_file_access"

    def collect(self, context: Dict[str, Any]) -> Optional[MetricResult]:
        image_path = context['image_path']
        tools = context.get('tools', {})
        dwarfs_path = tools.get('dwarfs')

        if not dwarfs_path:
            return None

        print(f"  Testing small file access...")

        with tempfile.TemporaryDirectory() as mount_point:
            mount_cmd = f"{dwarfs_path} {image_path} {mount_point}"

            mount_proc = None
            try:
                mount_proc = subprocess.Popen(
                    mount_cmd.split(),
                    stdout=subprocess.DEVNULL,
                    stderr=subprocess.DEVNULL
                )
                time.sleep(2)

                # Find small files (<10KB)
                small_files = []
                for root, dirs, files in os.walk(mount_point):
                    for f in files:
                        path = Path(root) / f
                        try:
                            if path.stat().st_size < 10240:  # <10KB
                                small_files.append(path)
                                if len(small_files) >= 100:
                                    break
                        except:
                            continue
                    if len(small_files) >= 100:
                        break

                if not small_files:
                    print("    SKIP: No small files found")
                    return None

                # Access 50 random small files
                latencies = []
                start_all = time.time()

                for _ in range(min(50, len(small_files))):
                    file = random.choice(small_files)

                    start = time.time()
                    try:
                        with open(file, 'rb') as f:
                            f.read()  # Read entire file
                    except:
                        continue

                    latency_ms = (time.time() - start) * 1000
                    latencies.append(latency_ms)

                total_time = time.time() - start_all

                if latencies:
                    latencies.sort()
                    p50 = latencies[len(latencies) // 2]
                    files_per_sec = len(latencies) / total_time if total_time > 0 else 0

                    return MetricResult(
                        name="fuse_small_file_p50",
                        value=p50,
                        unit="milliseconds",
                        timestamp=time.time(),
                        metadata={
                            'files_per_sec': files_per_sec,
                            'samples': len(latencies)
                        }
                    )

                return None

            except Exception as e:
                print(f"    ERROR: {e}")
                return None
            finally:
                if mount_proc:
                    mount_proc.terminate()
                    mount_proc.wait(timeout=5)
                subprocess.run(['umount', mount_point],
                             capture_output=True)

        return None


class FUSESequentialThroughputCollector(IMetricCollector):
    """
    Single Responsibility: Measure sequential read throughput

    Tests streaming performance
    """

    def get_name(self) -> str:
        return "fuse_sequential"

    def collect(self, context: Dict[str, Any]) -> Optional[MetricResult]:
        image_path = context['image_path']
        tools = context.get('tools', {})
        dwarfs_path = tools.get('dwarfs')

        if not dwarfs_path:
            return None

        print(f"  Testing sequential throughput...")

        with tempfile.TemporaryDirectory() as mount_point:
            mount_cmd = f"{dwarfs_path} {image_path} {mount_point}"

            mount_proc = None
            try:
                mount_proc = subprocess.Popen(
                    mount_cmd.split(),
                    stdout=subprocess.DEVNULL,
                    stderr=subprocess.DEVNULL
                )
                time.sleep(2)

                # Find a reasonably large file
                target_file = None
                for root, dirs, files in os.walk(mount_point):
                    for f in files:
                        path = Path(root) / f
                        try:
                            size = path.stat().st_size
                            if size > 100 * 1024:  # >100KB
                                target_file = path
                                break
                        except:
                            continue
                    if target_file:
                        break

                if not target_file:
                    print("    SKIP: No suitable file for throughput test")
                    return None

                # Sequential read
                file_size = target_file.stat().st_size
                start = time.time()

                with open(target_file, 'rb') as f:
                    data = f.read()

                elapsed = time.time() - start
                mb_per_sec = (file_size / (1024 * 1024)) / elapsed if elapsed > 0 else 0

                return MetricResult(
                    name="fuse_sequential_throughput",
                    value=mb_per_sec,
                    unit="MB/s",
                    timestamp=time.time(),
                    metadata={
                        'file_size': file_size,
                        'elapsed': elapsed
                    }
                )

            except Exception as e:
                print(f"    ERROR: {e}")
                return None
            finally:
                if mount_proc:
                    mount_proc.terminate()
                    mount_proc.wait(timeout=5)
                subprocess.run(['umount', mount_point],
                             capture_output=True)

        return None


class BenchmarkExecutor:
    """
    Single Responsibility: Orchestrate benchmark execution

    Uses collector classes (Strategy Pattern) to gather metrics
    Configuration-driven from YAML
    """

    def __init__(self, config_path: Path, metrics_path: Path):
        with open(config_path) as f:
            self.formats_config = yaml.safe_load(f)

        with open(metrics_path) as f:
            self.metrics_config = yaml.safe_load(f)

        # Registry of collectors (Open/Closed - easy to add)
        self.collectors: List[IMetricCollector] = [
            BuildTimeCollector(),
            ImageSizeCollector(),
            CompressionRatioCollector(),
            ExtractionTimeCollector(),
            FUSERandomAccessCollector(),
            FUSEFileCountCollector(),
            FUSELargeFileAccessCollector(),
            FUSESmallFileAccessCollector(),
            FUSESequentialThroughputCollector(),
        ]

    def _get_source_size(self, dataset_path: Path) -> int:
        """Calculate total size of source dataset"""
        total = 0
        for root, dirs, files in os.walk(dataset_path):
            for file in files:
                file_path = Path(root) / file
                if file_path.exists():
                    total += file_path.stat().st_size
        return total

    def _build_create_command(self,
                              format_config: Dict,
                              dataset_path: Path,
                              image_path: Path,
                              tools: Dict) -> str:
        """Build mkdwarfs command for format"""

        cmd_parts = [
            tools['mkdwarfs'],
            '-i', str(dataset_path),
            '-o', str(image_path),
            '--metadata-format', format_config['metadata_format'],
        ]

        # Add common args from config
        common = self.formats_config.get('common_args', {})
        if 'compression' in common:
            cmd_parts.extend(['--compression', common['compression']])
        if 'block_size' in common:
            cmd_parts.extend(['--block-size', str(common['block_size'])])

        return ' '.join(cmd_parts)

    def _build_extract_command(self,
                               image_path: Path,
                               extract_dir: Path,
                               tools: Dict) -> str:
        """Build dwarfsextract command"""

        return f"{tools['dwarfsextract']} -i {image_path} -o {extract_dir}"

    def _validate_tools(self, tools: Dict) -> bool:
        """Validate all required tools exist"""

        for name, path in tools.items():
            tool_path = Path(path).resolve()
            if not tool_path.exists():
                print(f"ERROR: Tool not found: {name} at {path}")
                return False

            # Test execution
            try:
                result = subprocess.run([str(tool_path), '--version'],
                                      capture_output=True,
                                      timeout=5,
                                      text=True)
                print(f"✓ {name}: {path}")
            except Exception as e:
                print(f"ERROR: Cannot execute {name}: {e}")
                return False

        return True

    def run_format_benchmark(self,
                            format_config: Dict,
                            dataset_path: Path,
                            tools: Dict,
                            output_dir: Path) -> BenchmarkResult:
        """
        Run complete benchmark for one format

        Returns BenchmarkResult with all metrics
        """

        format_name = format_config['name']
        format_slug = format_config['slug']

        print(f"\n{'='*60}")
        print(f"Benchmarking: {format_name}")
        print(f"{'='*60}")

        # Setup paths
        image_path = output_dir / f"{format_slug}.dwarfs"
        extract_dir = output_dir / f"{format_slug}_extracted"

        # Prepare context
        source_size = self._get_source_size(dataset_path)

        context = {
            'format': format_config,
            'dataset_path': dataset_path,
            'image_path': image_path,
            'extract_dir': extract_dir,
            'source_size': source_size,
            'tools': tools,
            'create_cmd': self._build_create_command(
                format_config, dataset_path, image_path, tools),
            'extract_cmd': self._build_extract_command(
                image_path, extract_dir, tools),
        }

        # Collect metrics
        results = []
        collected_metrics = {}

        for collector in self.collectors:
            print(f"\nCollecting: {collector.get_name()}")

            # Pass previous results in context
            context.update(collected_metrics)

            result = collector.collect(context)
            if result:
                results.append(result)
                collected_metrics[result.name + '_result'] = result
                print(f"  ✓ {result.name}: {result.value} {result.unit}")
            else:
                print(f"  ✗ Failed to collect {collector.get_name()}")

        return BenchmarkResult(
            format_name=format_name,
            dataset_name="raspios-arm64-lite",
            metrics=results,
            image_path=str(image_path),
            success=len(results) > 0
        )

    def run_all_formats(self,
                       dataset_path: Path,
                       tools: Dict,
                       output_dir: Path) -> List[BenchmarkResult]:
        """Run benchmarks for all configured formats"""

        results = []

        for format_config in self.formats_config['formats']:
            result = self.run_format_benchmark(
                format_config, dataset_path, tools, output_dir)
            results.append(result)

        return results

    def save_results(self, results: List[BenchmarkResult], output_path: Path):
        """Save results as JSON"""

        data = {
            'timestamp': time.time(),
            'results': [asdict(r) for r in results]
        }

        with open(output_path, 'w') as f:
            json.dump(data, f, indent=2)

        print(f"\nResults saved to: {output_path}")


def main():
    import argparse

    parser = argparse.ArgumentParser(
        description='DwarFS Format Benchmark Executor')

    parser.add_argument('--config', required=True, type=Path,
                       help='Path to dwarfs_formats.yaml')
    parser.add_argument('--metrics', required=True, type=Path,
                       help='Path to metrics.yaml')
    parser.add_argument('--dataset', required=True, type=Path,
                       help='Path to prepared dataset directory')
    parser.add_argument('--mkdwarfs', required=True, type=Path,
                       help='Path to mkdwarfs executable')
    parser.add_argument('--dwarfsck', required=True, type=Path,
                       help='Path to dwarfsck executable')
    parser.add_argument('--dwarfsextract', required=True, type=Path,
                       help='Path to dwarfsextract executable')
    parser.add_argument('--dwarfs', type=Path,
                       help='Path to dwarfs FUSE mount executable')
    parser.add_argument('--output', required=True, type=Path,
                       help='Output JSON file for results')
    parser.add_argument('--work-dir', type=Path, default=Path('/tmp/dwarfs_bench'),
                       help='Working directory for images')

    args = parser.parse_args()

    # Create working directory
    args.work_dir.mkdir(parents=True, exist_ok=True)

    # Setup tools
    tools = {
        'mkdwarfs': str(args.mkdwarfs),
        'dwarfsck': str(args.dwarfsck),
        'dwarfsextract': str(args.dwarfsextract),
        'dwarfs': str(args.dwarfs) if args.dwarfs else None
    }

    # Run benchmarks
    executor = BenchmarkExecutor(args.config, args.metrics)

    # Validate tools
    print("Validating tools...")
    if not executor._validate_tools(tools):
        print("ERROR: Tool validation failed")
        return 1

    print("\nStarting benchmark execution...")
    results = executor.run_all_formats(args.dataset, tools, args.work_dir)

    # Save results
    executor.save_results(results, args.output)

    print("\n" + "="*60)
    print("Benchmark execution complete!")
    print(f"Results: {args.output}")
    print("="*60)

    return 0


if __name__ == '__main__':
    exit(main())