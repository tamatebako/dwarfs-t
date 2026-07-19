#!/usr/bin/env python3
"""
DwarFS Metadata Format Benchmark Suite

Comprehensive benchmarking of Thrift and FlatBuffers metadata formats
across multiple datasets and operation types.

Measures:
- Compression (time, size, memory)
- Extraction (all files, single file)
- FUSE operations (mount, read, latency)

Outputs JSON results for report generation.
"""

import json
import os
import platform
import statistics
import subprocess
import sys
import tempfile
import time
from dataclasses import dataclass, field, asdict
from pathlib import Path
from typing import Dict, Any, List, Optional

# Import shared library utilities
sys.path.insert(0, str(Path(__file__).parent / 'lib'))
from memory_tracker import MemoryTracker
from perfmon_parser import PerfmonParser, LatencyMetric
from fuse_manager import FUSEManager
from progress import ProgressBar, print_header, print_status


@dataclass
class SystemMetadata:
    """System information for benchmark context"""
    timestamp: str
    hostname: str
    cpu: str
    memory: str
    os: str
    dwarfs_version: str
    fuse_implementation: str


@dataclass
class DatasetInfo:
    """Dataset information"""
    name: str
    source_path: str
    source_size_bytes: int
    file_count: int
    test_file: str


@dataclass
class CompressionMetrics:
    """Compression benchmark results"""
    time_seconds: float
    size_bytes: int
    memory_mb: float
    std_dev_time: float = 0.0
    std_dev_memory: float = 0.0


@dataclass
class ExtractionMetrics:
    """Extraction benchmark results"""
    time_seconds: float
    throughput_mbs: float
    memory_mb: float
    std_dev_time: float = 0.0
    std_dev_throughput: float = 0.0
    std_dev_memory: float = 0.0


@dataclass
class SingleFileMetrics:
    """Single file extraction/read metrics"""
    file: str
    time_seconds: float
    throughput_mbs: float
    memory_mb: float
    std_dev_time: float = 0.0
    std_dev_throughput: float = 0.0
    std_dev_memory: float = 0.0


@dataclass
class FUSEMetrics:
    """FUSE benchmark results"""
    mount_time_seconds: float
    init_time_seconds: float
    read_single: SingleFileMetrics
    read_all: ExtractionMetrics
    latency: Dict[str, LatencyMetric]
    std_dev_mount: float = 0.0
    std_dev_init: float = 0.0


@dataclass
class FormatResult:
    """Complete results for one format/dataset combination"""
    dataset: str
    format: str
    compression: CompressionMetrics
    extract_all: ExtractionMetrics
    extract_single: SingleFileMetrics
    fuse: FUSEMetrics


class MetadataFormatBenchmark:
    """Main benchmark orchestrator"""

    FORMATS = ['thrift', 'flatbuffers']

    def __init__(self, mkdwarfs: str, dwarfsextract: str, dwarfs: str, work_dir: Path, runs: int = 3):
        self.mkdwarfs = mkdwarfs
        self.dwarfsextract = dwarfsextract
        self.dwarfs = dwarfs
        self.work_dir = work_dir
        self.runs = runs
        self.memory_tracker = MemoryTracker()
        self.fuse_manager = FUSEManager(dwarfs)

        # Create work directory
        self.work_dir.mkdir(parents=True, exist_ok=True)

    def get_system_metadata(self) -> SystemMetadata:
        """Collect system information"""
        # Get DwarFS version (from stdout output without --version flag)
        version_result = subprocess.run(
            [self.mkdwarfs],
            capture_output=True,
            text=True
        )
        # Parse version from output like: "mkdwarfs (v0.14.1-11-g6c8300a43c-dirty ...)"
        import re
        lines = version_result.stdout.split('\n')
        version_line = lines[5] if len(lines) > 5 else ''
        match = re.search(r'mkdwarfs \((v[^\)]+)\)', version_line)
        dwarfs_version = match.group(1) if match else 'unknown'

        # Get FUSE implementation
        if platform.system() == 'Darwin':
            fuse_impl = 'fuse-t'
            try:
                fuse_result = subprocess.run(
                    ['mount_fuse-t', '--version'],
                    capture_output=True,
                    text=True
                )
                if fuse_result.returncode == 0:
                    fuse_impl = fuse_result.stdout.strip().split('\n')[0]
            except:
                pass
        else:
            fuse_impl = 'libfuse'

        return SystemMetadata(
            timestamp=time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime()),
            hostname=platform.node(),
            cpu=platform.processor() or 'unknown',
            memory=self._get_memory_info(),
            os=f"{platform.system()} {platform.release()}",
            dwarfs_version=dwarfs_version,
            fuse_implementation=fuse_impl
        )

    def _get_memory_info(self) -> str:
        """Get system memory info"""
        try:
            if platform.system() == 'Darwin':
                result = subprocess.run(
                    ['sysctl', '-n', 'hw.memsize'],
                    capture_output=True,
                    text=True
                )
                if result.returncode == 0:
                    bytes_val = int(result.stdout.strip())
                    gb = bytes_val / (1024 ** 3)
                    return f"{gb:.0f}GB"
            else:
                with open('/proc/meminfo') as f:
                    for line in f:
                        if line.startswith('MemTotal'):
                            kb = int(line.split()[1])
                            gb = kb / (1024 ** 2)
                            return f"{gb:.0f}GB"
        except:
            pass
        return 'unknown'

    def get_dataset_info(self, name: str, source_path: Path, test_file: str) -> DatasetInfo:
        """Collect dataset information"""
        # Calculate source size
        source_size = 0
        file_count = 0

        if source_path.is_file():
            source_size = source_path.stat().st_size
            file_count = 1
        else:
            for root, dirs, files in os.walk(source_path):
                for f in files:
                    fp = Path(root) / f
                    if fp.exists():
                        source_size += fp.stat().st_size
                        file_count += 1

        return DatasetInfo(
            name=name,
            source_path=str(source_path),
            source_size_bytes=source_size,
            file_count=file_count,
            test_file=test_file
        )

    def benchmark_compression(self, dataset_path: Path, format_name: str, output_path: Path) -> CompressionMetrics:
        """Benchmark filesystem compression"""
        print(f"  Compressing with {format_name}...")

        # Use --format option to select metadata serialization format
        cmd = f"{self.mkdwarfs} -i {dataset_path} -o {output_path} --format={format_name} --force"

        result = self.memory_tracker.measure_command(cmd)

        if result['returncode'] != 0:
            raise RuntimeError(f"Compression failed: {result['stderr']}")

        size_bytes = output_path.stat().st_size

        return CompressionMetrics(
            time_seconds=result['wall_time'],
            size_bytes=size_bytes,
            memory_mb=result['memory_mb']
        )

    def benchmark_extract_all(self, image_path: Path, extract_dir: Path, source_size: int) -> ExtractionMetrics:
        """Benchmark full extraction"""
        print(f"  Extracting all files...")

        # Create extract directory
        extract_dir.mkdir(parents=True, exist_ok=True)

        cmd = f"{self.dwarfsextract} -i {image_path} -o {extract_dir}"

        result = self.memory_tracker.measure_command(cmd)

        if result['returncode'] != 0:
            raise RuntimeError(f"Extraction failed: {result['stderr']}")

        throughput_mbs = (source_size / (1024 * 1024)) / result['wall_time'] if result['wall_time'] > 0 else 0

        return ExtractionMetrics(
            time_seconds=result['wall_time'],
            throughput_mbs=throughput_mbs,
            memory_mb=result['memory_mb']
        )

    def benchmark_extract_single(self, image_path: Path, test_file: str, extract_dir: Path) -> SingleFileMetrics:
        """Benchmark single file extraction"""
        print(f"  Extracting single file: {test_file}...")

        # Create extract directory
        single_dir = extract_dir / "single"
        single_dir.mkdir(parents=True, exist_ok=True)

        cmd = f"{self.dwarfsextract} -i {image_path} --pattern '{test_file}' -o {single_dir}"

        result = self.memory_tracker.measure_command(cmd)

        if result['returncode'] != 0:
            raise RuntimeError(f"Single file extraction failed: {result['stderr']}")

        # Get file size
        extracted_file = single_dir / test_file
        if extracted_file.exists():
            file_size = extracted_file.stat().st_size
            throughput_mbs = (file_size / (1024 * 1024)) / result['wall_time'] if result['wall_time'] > 0 else 0
        else:
            file_size = 0
            throughput_mbs = 0

        return SingleFileMetrics(
            file=test_file,
            time_seconds=result['wall_time'],
            throughput_mbs=throughput_mbs,
            memory_mb=result['memory_mb']
        )

    def benchmark_fuse_operations(self, image_path: Path, test_file: str, source_size: int) -> FUSEMetrics:
        """Benchmark FUSE mount and operations"""
        print(f"  Testing FUSE operations...")

        with tempfile.TemporaryDirectory() as mount_point:
            # Mount filesystem
            mount_start = time.time()
            proc = self.fuse_manager.mount(str(image_path), mount_point, perfmon=True)
            mount_time = time.time() - mount_start

            try:
                # Wait for init to complete
                time.sleep(0.5)
                init_time = time.time() - mount_start

                # Read single file
                read_single = self._fuse_read_single_file(mount_point, test_file)

                # Read all files
                read_all = self._fuse_read_all_files(mount_point, source_size)

                # Get latency metrics from perfmon
                perfmon_text = self.fuse_manager.get_perfmon_data(mount_point)
                latency = PerfmonParser.parse(perfmon_text)

                return FUSEMetrics(
                    mount_time_seconds=mount_time,
                    init_time_seconds=init_time,
                    read_single=read_single,
                    read_all=read_all,
                    latency=latency
                )

            finally:
                # Unmount
                self.fuse_manager.unmount(mount_point, proc)

    def _fuse_read_single_file(self, mount_point: str, test_file: str) -> SingleFileMetrics:
        """Read single file via FUSE"""
        file_path = Path(mount_point) / test_file

        if not file_path.exists():
            raise RuntimeError(f"Test file not found: {test_file}")

        file_size = file_path.stat().st_size

        start = time.time()
        with open(file_path, 'rb') as f:
            data = f.read()
        elapsed = time.time() - start

        throughput_mbs = (file_size / (1024 * 1024)) / elapsed if elapsed > 0 else 0

        return SingleFileMetrics(
            file=test_file,
            time_seconds=elapsed,
            throughput_mbs=throughput_mbs,
            memory_mb=0  # Not tracking memory for individual reads
        )

    def _fuse_read_all_files(self, mount_point: str, source_size: int) -> ExtractionMetrics:
        """Read all files via FUSE"""
        cmd = f"find {mount_point} -type f -print0 | xargs -0 cat > /dev/null"

        result = self.memory_tracker.measure_command(cmd)

        throughput_mbs = (source_size / (1024 * 1024)) / result['wall_time'] if result['wall_time'] > 0 else 0

        return ExtractionMetrics(
            time_seconds=result['wall_time'],
            throughput_mbs=throughput_mbs,
            memory_mb=result['memory_mb']
        )

    def run_single_benchmark(self, dataset_name: str, dataset_path: Path, test_file: str, format_name: str) -> FormatResult:
        """Run complete benchmark for one format/dataset with multiple runs"""
        print(f"\n{'='*60}")
        print(f"Benchmarking: {format_name} on {dataset_name} ({self.runs} runs)")
        print(f"{'='*60}")

        # Setup paths
        image_path = self.work_dir / f"{dataset_name}_{format_name}.dwarfs"
        extract_dir = self.work_dir / f"{dataset_name}_{format_name}_extracted"

        # Get dataset info
        dataset_info = self.get_dataset_info(dataset_name, dataset_path, test_file)

        # Run multiple iterations
        compression_results = []
        extract_all_results = []
        extract_single_results = []
        fuse_results = []

        for run_num in range(self.runs):
            print(f"\n  Run {run_num + 1}/{self.runs}...")

            # Clean up from previous runs
            if image_path.exists():
                image_path.unlink()
            if extract_dir.exists():
                import shutil
                shutil.rmtree(extract_dir)

            # Run benchmarks
            compression = self.benchmark_compression(dataset_path, format_name, image_path)
            extract_all = self.benchmark_extract_all(image_path, extract_dir, dataset_info.source_size_bytes)
            extract_single = self.benchmark_extract_single(image_path, test_file, extract_dir)
            fuse = self.benchmark_fuse_operations(image_path, test_file, dataset_info.source_size_bytes)

            compression_results.append(compression)
            extract_all_results.append(extract_all)
            extract_single_results.append(extract_single)
            fuse_results.append(fuse)

        # Average results (skip first run as warm-up if multiple runs)
        if self.runs > 1:
            print(f"\n  Averaging results (discarding first warm-up run)...")
            compression = self._average_compression_metrics(compression_results[1:])
            extract_all = self._average_extraction_metrics(extract_all_results[1:])
            extract_single = self._average_single_file_metrics(extract_single_results[1:])
            fuse = self._average_fuse_metrics(fuse_results[1:])
        else:
            compression = compression_results[0]
            extract_all = extract_all_results[0]
            extract_single = extract_single_results[0]
            fuse = fuse_results[0]

        return FormatResult(
            dataset=dataset_name,
            format=format_name,
            compression=compression,
            extract_all=extract_all,
            extract_single=extract_single,
            fuse=fuse
        )

    def _average_compression_metrics(self, results: List[CompressionMetrics]) -> CompressionMetrics:
        """Average compression metrics across multiple runs"""
        times = [r.time_seconds for r in results]
        memories = [r.memory_mb for r in results]

        return CompressionMetrics(
            time_seconds=statistics.mean(times),
            size_bytes=results[0].size_bytes,  # Size should be identical
            memory_mb=statistics.mean(memories),
            std_dev_time=statistics.stdev(times) if len(times) > 1 else 0.0,
            std_dev_memory=statistics.stdev(memories) if len(memories) > 1 else 0.0
        )

    def _average_extraction_metrics(self, results: List[ExtractionMetrics]) -> ExtractionMetrics:
        """Average extraction metrics across multiple runs"""
        times = [r.time_seconds for r in results]
        throughputs = [r.throughput_mbs for r in results]
        memories = [r.memory_mb for r in results]

        return ExtractionMetrics(
            time_seconds=statistics.mean(times),
            throughput_mbs=statistics.mean(throughputs),
            memory_mb=statistics.mean(memories),
            std_dev_time=statistics.stdev(times) if len(times) > 1 else 0.0,
            std_dev_throughput=statistics.stdev(throughputs) if len(throughputs) > 1 else 0.0,
            std_dev_memory=statistics.stdev(memories) if len(memories) > 1 else 0.0
        )

    def _average_single_file_metrics(self, results: List[SingleFileMetrics]) -> SingleFileMetrics:
        """Average single file metrics across multiple runs"""
        times = [r.time_seconds for r in results]
        throughputs = [r.throughput_mbs for r in results]
        memories = [r.memory_mb for r in results]

        return SingleFileMetrics(
            file=results[0].file,
            time_seconds=statistics.mean(times),
            throughput_mbs=statistics.mean(throughputs),
            memory_mb=statistics.mean(memories),
            std_dev_time=statistics.stdev(times) if len(times) > 1 else 0.0,
            std_dev_throughput=statistics.stdev(throughputs) if len(throughputs) > 1 else 0.0,
            std_dev_memory=statistics.stdev(memories) if len(memories) > 1 else 0.0
        )

    def _average_fuse_metrics(self, results: List[FUSEMetrics]) -> FUSEMetrics:
        """Average FUSE metrics across multiple runs"""
        mount_times = [r.mount_time_seconds for r in results]
        init_times = [r.init_time_seconds for r in results]

        # Average read metrics
        read_single_list = [r.read_single for r in results]
        read_all_list = [r.read_all for r in results]

        return FUSEMetrics(
            mount_time_seconds=statistics.mean(mount_times),
            init_time_seconds=statistics.mean(init_times),
            read_single=self._average_single_file_metrics(read_single_list),
            read_all=self._average_extraction_metrics(read_all_list),
            latency=results[0].latency,  # Use first run's latency (perfmon data)
            std_dev_mount=statistics.stdev(mount_times) if len(mount_times) > 1 else 0.0,
            std_dev_init=statistics.stdev(init_times) if len(init_times) > 1 else 0.0
        )

    def run_all_benchmarks(self, datasets: Dict[str, Dict[str, Any]]) -> Dict[str, Any]:
        """Run benchmarks for all formats and datasets"""
        metadata = self.get_system_metadata()

        # Collect dataset info
        dataset_infos = {}
        for name, info in datasets.items():
            dataset_infos[name] = asdict(
                self.get_dataset_info(name, Path(info['path']), info['test_file'])
            )

        # Run benchmarks
        results = []
        for dataset_name, dataset_info in datasets.items():
            for format_name in self.FORMATS:
                try:
                    result = self.run_single_benchmark(
                        dataset_name,
                        Path(dataset_info['path']),
                        dataset_info['test_file'],
                        format_name
                    )
                    results.append(asdict(result))
                except Exception as e:
                    print(f"ERROR: Benchmark failed: {e}")
                    continue

        return {
            'metadata': asdict(metadata),
            'datasets': dataset_infos,
            'results': results
        }


def main():
    import argparse

    parser = argparse.ArgumentParser(
        description='DwarFS Metadata Format Benchmark Suite'
    )

    parser.add_argument('--mkdwarfs', required=True, type=Path,
                       help='Path to mkdwarfs')
    parser.add_argument('--dwarfsextract', required=True, type=Path,
                       help='Path to dwarfsextract')
    parser.add_argument('--dwarfs', required=True, type=Path,
                       help='Path to dwarfs FUSE mount')
    parser.add_argument('--perl-dataset', required=True, type=Path,
                       help='Path to Perl dataset directory')
    parser.add_argument('--raspios-dataset', type=Path,
                       help='Path to RaspOS dataset directory (optional)')
    parser.add_argument('--output', required=True, type=Path,
                       help='Output JSON file')
    parser.add_argument('--work-dir', type=Path, default=Path('/tmp/metadata_bench'),
                       help='Working directory for images')
    parser.add_argument('--runs', type=int, default=3,
                       help='Number of runs per test (default: 3, first run discarded as warm-up)')

    args = parser.parse_args()

    # Setup datasets
    datasets = {
        'perl': {
            'path': str(args.perl_dataset),
            'test_file': 'doio.c'
        }
    }

    if args.raspios_dataset:
        datasets['raspios'] = {
            'path': str(args.raspios_dataset),
            'test_file': 'raspios_sample.img'
        }

    # Run benchmarks
    benchmark = MetadataFormatBenchmark(
        str(args.mkdwarfs),
        str(args.dwarfsextract),
        str(args.dwarfs),
        args.work_dir,
        runs=args.runs
    )

    print("Starting metadata format benchmark suite...")
    print(f"Datasets: {', '.join(datasets.keys())}")
    print(f"Formats: {', '.join(benchmark.FORMATS)}")
    print(f"Runs per test: {args.runs} (first run is warm-up, discarded from averages)")

    results = benchmark.run_all_benchmarks(datasets)

    # Save results
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with open(args.output, 'w') as f:
        json.dump(results, f, indent=2, default=lambda o: asdict(o) if hasattr(o, '__dataclass_fields__') else str(o))

    print(f"\n{'='*60}")
    print(f"Benchmark complete!")
    print(f"Results saved to: {args.output}")
    print(f"{'='*60}")

    return 0


if __name__ == '__main__':
    sys.exit(main())