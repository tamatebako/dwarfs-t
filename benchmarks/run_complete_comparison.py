#!/usr/bin/env python3
"""
Complete DwarFS Format Comparison Benchmark

Tests 3 build configurations:
1. Dual-format (THRIFT=ON, FLATBUFFERS=ON) - Both available
2. FlatBuffers-only (THRIFT=OFF, FLATBUFFERS=ON)
3. Thrift-only (THRIFT=ON, FLATBUFFERS=OFF) - Legacy mode

Generates comprehensive JSON results and Markdown report.
"""

import argparse
import json
import os
import subprocess
import sys
import tempfile
import time
from pathlib import Path
from typing import Dict, List, Optional

# Add lib to path
sys.path.insert(0, str(Path(__file__).parent / 'lib'))

from memory_tracker import MemoryTracker
from fuse_manager import FUSEManager
from perfmon_parser import PerfmonParser


class BuildConfig:
    """Represents a build configuration."""
    def __init__(self, name: str, thrift: bool, flatbuffers: bool):
        self.name = name
        self.thrift_enabled = thrift
        self.flatbuffers_enabled = flatbuffers
        self.build_dir = f"build-{name}"


CONFIGS = [
    BuildConfig("dual", thrift=True, flatbuffers=True),
    BuildConfig("flatbuffers", thrift=False, flatbuffers=True),
    BuildConfig("thrift", thrift=True, flatbuffers=False),
]


def build_configuration(config: BuildConfig, source_dir: Path, jobs: int = 4) -> bool:
    """Build a specific configuration."""
    print(f"\n=== Building {config.name} configuration ===")
    print(f"THRIFT={config.thrift_enabled}, FLATBUFFERS={config.flatbuffers_enabled}")
    
    # Configure
    cmake_cmd = [
        "cmake", "-B", config.build_dir, "-GNinja",
        "-DCMAKE_BUILD_TYPE=Release",
        f"-DDWARFS_WITH_THRIFT={'ON' if config.thrift_enabled else 'OFF'}",
        f"-DDWARFS_WITH_FLATBUFFERS={'ON' if config.flatbuffers_enabled else 'OFF'}",
        "-DWITH_TOOLS=ON",
        "-DWITH_TESTS=OFF",
    ]
    
    print(f"$ {' '.join(cmake_cmd)}")
    result = subprocess.run(cmake_cmd, cwd=source_dir, capture_output=True, text=True)
    
    if result.returncode != 0:
        print(f"CMake failed:")
        print(result.stderr)
        return False
    
    # Build
    build_cmd = ["ninja", "-C", config.build_dir, f"-j{jobs}"]
    print(f"$ {' '.join(build_cmd)}")
    result = subprocess.run(build_cmd, cwd=source_dir, capture_output=True, text=True)
    
    if result.returncode != 0:
        print(f"Build failed:")
        print(result.stderr)
        return False
    
    print(f"✅ {config.name} build successful")
    return True


def benchmark_image_creation(
    mkdwarfs: Path,
    input_dir: Path,
    output_file: Path,
    format_name: str,
    tracker: MemoryTracker
) -> Dict:
    """Benchmark filesystem image creation."""
    print(f"\n  Creating image with {format_name}...")
    
    cmd = [
        str(mkdwarfs),
        "-i", str(input_dir),
        "-o", str(output_file),
        f"--metadata-format={format_name}",
        "--log-level=warn",
    ]
    
    start = time.time()
    result = tracker.measure_command(cmd)
    build_time = time.time() - start
    
    image_size = output_file.stat().st_size if output_file.exists() else 0
    
    return {
        "build_time_sec": build_time,
        "image_size_bytes": image_size,
        "memory_mb": result.get("memory_mb", 0),
        "success": result["returncode"] == 0
    }


def benchmark_extraction(
    dwarfsextract: Path,
    image_file: Path,
    output_dir: Path,
    tracker: MemoryTracker
) -> Dict:
    """Benchmark full filesystem extraction."""
    print(f"  Extracting image...")
    
    cmd = [
        str(dwarfsextract),
        "-i", str(image_file),
        "-o", str(output_dir),
    ]
    
    start = time.time()
    result = tracker.measure_command(cmd)
    extract_time = time.time() - start
    
    return {
        "extract_time_sec": extract_time,
        "memory_mb": result.get("memory_mb", 0),
        "success": result["returncode"] == 0
    }


def benchmark_fuse_mount(
    dwarfs: Path,
    image_file: Path,
    test_operations: int = 100,
    tracker: Optional[MemoryTracker] = None
) -> Dict:
    """Benchmark FUSE mount and operations."""
    print(f"  Mounting and testing FUSE...")
    
    manager = FUSEManager(str(dwarfs))
    
    with tempfile.TemporaryDirectory() as mount_point:
        # Mount
        mount_start = time.time()
        proc = manager.mount(str(image_file), mount_point, perfmon=True)
        mount_time = time.time() - mount_start
        
        if not proc:
            return {"success": False, "error": "Mount failed"}
        
        # Random access test
        find_start = time.time()
        find_result = subprocess.run(
            ["find", mount_point, "-type", "f"],
            capture_output=True, text=True
        )
        find_time = time.time() - find_start
        file_count = len(find_result.stdout.strip().split('\n')) if find_result.returncode == 0 else 0
        
        # Get perfmon data
        perfmon_text = manager.get_perfmon_xattr(mount_point)
        latency = PerfmonParser.parse(perfmon_text) if perfmon_text else {}
        
        # Unmount
        manager.unmount(mount_point, proc)
        
        return {
            "mount_time_sec": mount_time,
            "find_time_sec": find_time,
            "file_count": file_count,
            "latency_p50_us": latency.get("op_read", {}).get("p50_us", 0),
            "latency_p99_us": latency.get("op_read", {}).get("p99_us", 0),
            "success": True
        }


def run_benchmark_suite(
    config: BuildConfig,
    source_dir: Path,
    dataset: Path,
    runs: int = 3
) -> Dict:
    """Run complete benchmark suite for one configuration."""
    print(f"\n{'='*60}")
    print(f"Benchmarking: {config.name}")
    print(f"{'='*60}")
    
    build_dir = source_dir / config.build_dir
    mkdwarfs = build_dir / "mkdwarfs"
    dwarfsextract = build_dir / "dwarfsextract"
    dwarfs = build_dir / "dwarfs"
    
    # Check executables exist
    for exe in [mkdwarfs, dwarfsextract, dwarfs]:
        if not exe.exists():
            return {"error": f"Executable not found: {exe}"}
    
    tracker = MemoryTracker()
    results = {
        "config": config.name,
        "thrift_enabled": config.thrift_enabled,
        "flatbuffers_enabled": config.flatbuffers_enabled,
        "runs": []
    }
    
    for run in range(runs):
        print(f"\n--- Run {run + 1}/{runs} ---")
        run_result = {}
        
        with tempfile.TemporaryDirectory() as work_dir:
            work_path = Path(work_dir)
            
            # Test each available format
            formats_to_test = []
            if config.flatbuffers_enabled:
                formats_to_test.append("flatbuffers")
            if config.thrift_enabled:
                formats_to_test.append("thrift")
            
            run_result["formats"] = {}
            
            for fmt in formats_to_test:
                print(f"\n Format: {fmt}")
                image_file = work_path / f"test_{fmt}.dwarfs"
                extract_dir = work_path / f"extract_{fmt}"
                
                # Build image
                build_result = benchmark_image_creation(
                    mkdwarfs, dataset, image_file, fmt, tracker
                )
                
                if not build_result["success"]:
                    print(f"  ❌ Build failed")
                    continue
                
                # Extract
                extract_result = benchmark_extraction(
                    dwarfsextract, image_file, extract_dir, tracker
                )
                
                # FUSE operations
                fuse_result = benchmark_fuse_mount(dwarfs, image_file, tracker=tracker)
                
                run_result["formats"][fmt] = {
                    "build": build_result,
                    "extract": extract_result,
                    "fuse": fuse_result
                }
                
                print(f"  ✅ Build: {build_result['build_time_sec']:.2f}s, "
                      f"Size: {build_result['image_size_bytes']/(1024*1024):.2f} MB")
        
        results["runs"].append(run_result)
    
    return results


def generate_report(all_results: Dict, output_path: Path):
    """Generate comprehensive Markdown report."""
    md = []
    md.append("# DwarFS Format Comparison Benchmark Results\n")
    md.append(f"**Generated**: {time.strftime('%Y-%m-%d %H:%M:%S')}\n")
    md.append(f"**Dataset**: Perl 5.43.3 (6,802 files, ~95 MB)\n\n")
    
    md.append("## Executive Summary\n\n")
    
    # Calculate averages for each config
    for config_name, config_results in all_results.items():
        if "error" in config_results:
            md.append(f"### {config_name}: ❌ {config_results['error']}\n\n")
            continue
        
        md.append(f"### {config_name.upper()} Build\n\n")
        md.append(f"- Thrift: {'ON' if config_results['thrift_enabled'] else 'OFF'}\n")
        md.append(f"- FlatBuffers: {'ON' if config_results['flatbuffers_enabled'] else 'OFF'}\n\n")
        
        # Aggregate results
        for fmt in ["flatbuffers", "thrift"]:
            fmt_data = []
            for run in config_results["runs"]:
                if fmt in run.get("formats", {}):
                    fmt_data.append(run["formats"][fmt])
            
            if not fmt_data:
                continue
            
            avg_build = sum(r["build"]["build_time_sec"] for r in fmt_data) / len(fmt_data)
            avg_size = sum(r["build"]["image_size_bytes"] for r in fmt_data) / len(fmt_data)
            avg_extract = sum(r["extract"]["extract_time_sec"] for r in fmt_data) / len(fmt_data)
            
            md.append(f"**{fmt.upper()} Format**:\n")
            md.append(f"- Build time: {avg_build:.2f}s\n")
            md.append(f"- Image size: {avg_size/(1024*1024):.2f} MB\n")
            md.append(f"- Extract time: {avg_extract:.2f}s\n\n")
    
    md.append("## Detailed Results\n\n")
    md.append("(Full benchmark data in JSON)\n\n")
    
    md.append("## Recommendations\n\n")
    md.append("- **FlatBuffers**: Modern default, portable, good performance\n")
    md.append("- **Thrift**: Smallest size, legacy compatibility\n")
    md.append("- **Dual**: Maximum flexibility, choose per-image\n\n")
    
    output_path.write_text(''.join(md))
    print(f"\n✅ Report generated: {output_path}")


def main():
    parser = argparse.ArgumentParser(description="DwarFS Complete Format Comparison")
    parser.add_argument("--source-dir", type=Path, default=Path.cwd(),
                       help="DwarFS source directory")
    parser.add_argument("--dataset", type=Path, required=True,
                       help="Test dataset directory (e.g., perl-5.43.3)")
    parser.add_argument("--output", type=Path, 
                       default=Path("benchmarks/results/complete_comparison.json"),
                       help="Output JSON file")
    parser.add_argument("--report", type=Path,
                       default=Path("benchmarks/results/flatbuffers_vs_thrift.md"),
                       help="Output Markdown report")
    parser.add_argument("--runs", type=int, default=3,
                       help="Number of runs per configuration")
    parser.add_argument("--skip-build", action="store_true",
                       help="Skip building (use existing binaries)")
    parser.add_argument("--jobs", type=int, default=4,
                       help="Parallel build jobs")
    
    args = parser.parse_args()
    
    if not args.dataset.exists():
        print(f"❌ Dataset not found: {args.dataset}")
        print(f"Download with: python3 benchmarks/download_datasets.py --download perl")
        return 1
    
    print(f"DwarFS Complete Format Comparison")
    print(f"Source: {args.source_dir}")
    print(f"Dataset: {args.dataset}")
    print(f"Runs: {args.runs}")
    
    all_results = {}
    
    # Build and benchmark each configuration
    for config in CONFIGS:
        # Build
        if not args.skip_build:
            if not build_configuration(config, args.source_dir, args.jobs):
                all_results[config.name] = {"error": "Build failed"}
                continue
        
        # Benchmark
        results = run_benchmark_suite(config, args.source_dir, args.dataset, args.runs)
        all_results[config.name] = results
    
    # Save JSON
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with open(args.output, 'w') as f:
        json.dump(all_results, f, indent=2)
    print(f"\n✅ Results saved: {args.output}")
    
    # Generate report
    generate_report(all_results, args.report)
    
    return 0


if __name__ == "__main__":
    sys.exit(main())
