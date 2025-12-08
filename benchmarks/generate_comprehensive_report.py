#!/usr/bin/env python3
"""
Comprehensive Report Generator for DwarFS Benchmarks

Reads the unified benchmark JSON and generates a comprehensive Markdown report
covering all tools, formats, and datasets.
"""

import argparse
import json
import sys
from datetime import datetime
from pathlib import Path
from typing import Dict, List, Optional


class ComprehensiveReportGenerator:
    """Generates comprehensive benchmark reports."""
    
    def __init__(self, results: Dict):
        self.results = results
        self.metadata = results.get('metadata', {})
        self.data = results.get('results', {})
    
    def generate(self) -> str:
        """Generate full Markdown report."""
        sections = [
            self._header(),
            self._executive_summary(),
            self._mkdwarfs_analysis(),
            self._dwarfsck_analysis(),
            self._dwarfsextract_analysis(),
            self._dwarfs_fuse_analysis(),
            self._format_comparison_matrix(),
            self._performance_profiles(),
            self._recommendations()
        ]
        
        return '\n\n'.join(filter(None, sections))
    
    def _header(self) -> str:
        """Generate report header."""
        return f"""# DwarFS Comprehensive Benchmark Report

**Date**: {self.metadata.get('date', 'Unknown')}  
**Platform**: {self.metadata.get('platform', 'Unknown')} {self.metadata.get('machine', '')}  
**Datasets**: {', '.join(self.metadata.get('datasets', []))}  
**Compression Levels**: {', '.join(map(str, self.metadata.get('compression_levels', [])))}  
**Iterations**: {self.metadata.get('iterations', 'Unknown')}

---"""
    
    def _executive_summary(self) -> str:
        """Generate executive summary with key findings."""
        lines = ["## Executive Summary"]
        
        # Analyze all results to find best performers
        best_creation_time = None
        best_creation_format = None
        smallest_image = None
        smallest_format = None
        fastest_verification = None
        fastest_verification_format = None
        
        for dataset_name, dataset_data in self.data.items():
            # mkdwarfs
            mkdwarfs_data = dataset_data.get('mkdwarfs', {})
            for format_name, format_data in mkdwarfs_data.items():
                for level, level_data in format_data.items():
                    runs = level_data.get('runs', [])
                    if runs and 'wall_time' in runs[0]:
                        avg_time = sum(r['wall_time'] for r in runs) / len(runs)
                        if best_creation_time is None or avg_time < best_creation_time:
                            best_creation_time = avg_time
                            best_creation_format = f"{format_name} {level}"
                        
                        avg_size = sum(r['image_size'] for r in runs if 'image_size' in r) / len(runs)
                        if smallest_image is None or avg_size < smallest_image:
                            smallest_image = avg_size
                            smallest_format = f"{format_name} {level}"
            
            # dwarfsck
            dwarfsck_data = dataset_data.get('dwarfsck', {})
            for image_key, image_data in dwarfsck_data.items():
                quick_runs = image_data.get('quick_check', {}).get('runs', [])
                if quick_runs and 'wall_time' in quick_runs[0]:
                    avg_time = sum(r['wall_time'] for r in quick_runs) / len(quick_runs)
                    if fastest_verification is None or avg_time < fastest_verification:
                        fastest_verification = avg_time
                        fastest_verification_format = image_key
        
        lines.append("\n### Key Findings\n")
        
        if best_creation_time:
            lines.append(f"- **Fastest Creation**: {best_creation_format} ({best_creation_time:.2f}s)")
        if smallest_image:
            lines.append(f"- **Smallest Image**: {smallest_format} ({smallest_image:,} bytes)")
        if fastest_verification:
            lines.append(f"- **Fastest Verification**: {fastest_verification_format} ({fastest_verification:.2f}s)")
        
        # Format comparison summary
        fb_count = sum(1 for d in self.data.values() if 'mkdwarfs' in d and 'flatbuffers' in d['mkdwarfs'])
        tb_count = sum(1 for d in self.data.values() if 'mkdwarfs' in d and 'thrift' in d['mkdwarfs'])
        
        lines.append(f"\n### Format Coverage")
        lines.append(f"- **FlatBuffers**: {fb_count} dataset(s) tested")
        lines.append(f"- **Thrift**: {tb_count} dataset(s) tested")
        
        return '\n'.join(lines)
    
    def _mkdwarfs_analysis(self) -> str:
        """Analyze mkdwarfs performance."""
        lines = ["## mkdwarfs Analysis (Creation)"]
        
        for dataset_name, dataset_data in self.data.items():
            mkdwarfs_data = dataset_data.get('mkdwarfs', {})
            if not mkdwarfs_data:
                continue
            
            lines.append(f"\n### Dataset: {dataset_name}")
            lines.append("\n| Format | Level | Time (s) | Image Size | Ratio | Throughput (MB/s) | Memory (MB) |")
            lines.append("|--------|-------|----------|------------|-------|-------------------|-------------|")
            
            for format_name in ['flatbuffers', 'thrift']:
                format_data = mkdwarfs_data.get(format_name, {})
                for level in sorted(format_data.keys()):
                    level_data = format_data[level]
                    runs = level_data.get('runs', [])
                    if not runs or 'wall_time' not in runs[0]:
                        continue
                    
                    avg_time = sum(r['wall_time'] for r in runs) / len(runs)
                    avg_size = sum(r['image_size'] for r in runs if 'image_size' in r) / len(runs)
                    avg_ratio = sum(r['compression_ratio'] for r in runs if 'compression_ratio' in r) / len(runs)
                    avg_throughput = sum(r['throughput_mbps'] for r in runs if 'throughput_mbps' in r) / len(runs)
                    avg_memory = sum(r['memory_mb'] for r in runs if 'memory_mb' in r) / len([r for r in runs if 'memory_mb' in r])
                    
                    lines.append(f"| {format_name} | {level} | {avg_time:.2f} | {avg_size:,.0f} | {avg_ratio:.1f}x | {avg_throughput:.1f} | {avg_memory:.1f} |")
        
        return '\n'.join(lines)
    
    def _dwarfsck_analysis(self) -> str:
        """Analyze dwarfsck performance."""
        lines = ["## dwarfsck Analysis (Verification)"]
        
        for dataset_name, dataset_data in self.data.items():
            dwarfsck_data = dataset_data.get('dwarfsck', {})
            if not dwarfsck_data:
                continue
            
            lines.append(f"\n### Dataset: {dataset_name}")
            lines.append("\n| Image | Quick Check (s) | Full Validation (s) | JSON Export (s) | JSON Size |")
            lines.append("|-------|-----------------|---------------------|-----------------|-----------|")
            
            for image_key, image_data in dwarfsck_data.items():
                quick_runs = image_data.get('quick_check', {}).get('runs', [])
                full_runs = image_data.get('full_validation', {}).get('runs', [])
                json_runs = image_data.get('json_export', {}).get('runs', [])
                
                quick_time = sum(r['wall_time'] for r in quick_runs) / len(quick_runs) if quick_runs else 0
                full_time = sum(r['wall_time'] for r in full_runs) / len(full_runs) if full_runs else 0
                json_time = sum(r['wall_time'] for r in json_runs) / len(json_runs) if json_runs else 0
                json_size = sum(r['json_size'] for r in json_runs) / len(json_runs) if json_runs else 0
                
                lines.append(f"| {image_key} | {quick_time:.2f} | {full_time:.2f} | {json_time:.2f} | {json_size:,.0f} |")
        
        return '\n'.join(lines)
    
    def _dwarfsextract_analysis(self) -> str:
        """Analyze dwarfsextract performance."""
        lines = ["## dwarfsextract Analysis (Extraction)"]
        
        for dataset_name, dataset_data in self.data.items():
            extract_data = dataset_data.get('dwarfsextract', {})
            if not extract_data:
                continue
            
            lines.append(f"\n### Dataset: {dataset_name}")
            lines.append("\n| Image | Extract All (s) | Throughput (MB/s) | To Tar (s) | Tar Throughput (MB/s) |")
            lines.append("|-------|-----------------|-------------------|------------|------------------------|")
            
            for image_key, image_data in extract_data.items():
                extract_runs = image_data.get('extract_all', {}).get('runs', [])
                tar_runs = image_data.get('to_tar', {}).get('runs', [])
                
                extract_time = sum(r['wall_time'] for r in extract_runs) / len(extract_runs) if extract_runs else 0
                extract_throughput = sum(r['throughput_mbps'] for r in extract_runs) / len(extract_runs) if extract_runs else 0
                tar_time = sum(r['wall_time'] for r in tar_runs) / len(tar_runs) if tar_runs else 0
                tar_throughput = sum(r['throughput_mbps'] for r in tar_runs) / len(tar_runs) if tar_runs else 0
                
                lines.append(f"| {image_key} | {extract_time:.2f} | {extract_throughput:.1f} | {tar_time:.2f} | {tar_throughput:.1f} |")
        
        return '\n'.join(lines)
    
    def _dwarfs_fuse_analysis(self) -> str:
        """Analyze dwarfs FUSE performance."""
        lines = ["## dwarfs FUSE Analysis (Mount & Access)"]
        
        has_fuse_data = False
        for dataset_data in self.data.values():
            if 'dwarfs' in dataset_data:
                has_fuse_data = True
                break
        
        if not has_fuse_data:
            lines.append("\n*FUSE benchmarks were not run (FUSE not available or not tested)*")
            return '\n'.join(lines)
        
        for dataset_name, dataset_data in self.data.items():
            fuse_data = dataset_data.get('dwarfs', {})
            if not fuse_data:
                continue
            
            lines.append(f"\n### Dataset: {dataset_name}")
            lines.append("\n| Image | Mount Time (s) | Seq Read (s) | Throughput (MB/s) | Find Time (s) | Files |")
            lines.append("|-------|----------------|--------------|-------------------|---------------|-------|")
            
            for image_key, image_data in fuse_data.items():
                mount_time = image_data.get('mount_time', 0)
                seq_read = image_data.get('seq_read', {})
                find_data = image_data.get('find', {})
                
                seq_time = seq_read.get('time', 0)
                seq_throughput = seq_read.get('throughput_mbps', 0)
                find_time = find_data.get('time', 0)
                file_count = find_data.get('file_count', 0)
                
                lines.append(f"| {image_key} | {mount_time:.3f} | {seq_time:.3f} | {seq_throughput:.1f} | {find_time:.3f} | {file_count} |")
        
        return '\n'.join(lines)
    
    def _format_comparison_matrix(self) -> str:
        """Generate format comparison matrix."""
        lines = ["## Format Comparison Matrix"]
        lines.append("\nComparing FlatBuffers vs Thrift across all operations:\n")
        
        # Collect comparison data
        comparisons = []
        
        for dataset_name, dataset_data in self.data.items():
            mkdwarfs_data = dataset_data.get('mkdwarfs', {})
            
            for level in self.metadata.get('compression_levels', []):
                level_key = f"l{level}"
                
                fb_data = mkdwarfs_data.get('flatbuffers', {}).get(level_key, {})
                tb_data = mkdwarfs_data.get('thrift', {}).get(level_key, {})
                
                fb_runs = fb_data.get('runs', [])
                tb_runs = tb_data.get('runs', [])
                
                if fb_runs and tb_runs and 'image_size' in fb_runs[0] and 'image_size' in tb_runs[0]:
                    fb_size = sum(r['image_size'] for r in fb_runs) / len(fb_runs)
                    tb_size = sum(r['image_size'] for r in tb_runs) / len(tb_runs)
                    size_ratio = (fb_size / tb_size * 100) if tb_size > 0 else 0
                    
                    fb_time = sum(r['wall_time'] for r in fb_runs) / len(fb_runs)
                    tb_time = sum(r['wall_time'] for r in tb_runs) / len(tb_runs)
                    time_ratio = (fb_time / tb_time * 100) if tb_time > 0 else 0
                    
                    comparisons.append({
                        'dataset': dataset_name,
                        'level': level,
                        'size_ratio': size_ratio,
                        'time_ratio': time_ratio
                    })
        
        if comparisons:
            lines.append("| Dataset | Level | Size Ratio (FB/TB) | Time Ratio (FB/TB) |")
            lines.append("|---------|-------|--------------------|--------------------|")
            
            for comp in comparisons:
                lines.append(f"| {comp['dataset']} | {comp['level']} | {comp['size_ratio']:.2f}% | {comp['time_ratio']:.2f}% |")
            
            # Calculate averages
            avg_size = sum(c['size_ratio'] for c in comparisons) / len(comparisons)
            avg_time = sum(c['time_ratio'] for c in comparisons) / len(comparisons)
            
            lines.append(f"| **Average** | - | **{avg_size:.2f}%** | **{avg_time:.2f}%** |")
            
            lines.append(f"\n**Interpretation**:")
            lines.append(f"- FlatBuffers images are **{avg_size:.1f}%** the size of Thrift (overhead: {avg_size - 100:.1f}%)")
            lines.append(f"- FlatBuffers creation is **{avg_time:.1f}%** the time of Thrift")
        else:
            lines.append("\n*No comparable data available*")
        
        return '\n'.join(lines)
    
    def _performance_profiles(self) -> str:
        """Generate performance profiles per dataset type."""
        lines = ["## Performance Profiles"]
        
        for dataset_name in self.metadata.get('datasets', []):
            lines.append(f"\n### {dataset_name.capitalize()} Dataset")
            
            dataset_data = self.data.get(dataset_name, {})
            if not dataset_data:
                lines.append("\n*No data available*")
                continue
            
            mkdwarfs = dataset_data.get('mkdwarfs', {})
            dwarfsck = dataset_data.get('dwarfsck', {})
            extract = dataset_data.get('dwarfsextract', {})
            fuse = dataset_data.get('dwarfs', {})
            
            lines.append(f"\n**Strengths**:")
            
            # Find best performers for this dataset
            if mkdwarfs:
                lines.append(f"- Image creation tested with {len(mkdwarfs)} format(s)")
            if dwarfsck:
                lines.append(f"- Verification tested on {len(dwarfsck)} image(s)")
            if extract:
                lines.append(f"- Extraction tested on {len(extract)} image(s)")
            if fuse:
                lines.append(f"- FUSE operations tested on {len(fuse)} image(s)")
            
            lines.append(f"\n**Recommendations**:")
            lines.append(f"- Best for: {'Small files workload' if dataset_name == 'perl' else 'Large files workload' if dataset_name == 'raspios' else 'Quick testing'}")
        
        return '\n'.join(lines)
    
    def _recommendations(self) -> str:
        """Generate recommendations."""
        lines = ["## Recommendations"]
        
        lines.append("\n### When to Use FlatBuffers")
        lines.append("- ✅ **Default choice** for new projects")
        lines.append("- ✅ Cross-platform portability requirements")
        lines.append("- ✅ Simpler build dependencies")
        lines.append("- ✅ Size overhead <110% of Thrift is acceptable")
        
        lines.append("\n### When to Keep Thrift Support")
        lines.append("- ⚠️ Reading legacy images created with Thrift")
        lines.append("- ⚠️ Absolute maximum compression required")
        lines.append("- ⚠️ Have Folly/fbthrift build infrastructure")
        
        lines.append("\n### Performance Tuning Tips")
        lines.append("- **Level 1**: Fastest creation, moderate compression")
        lines.append("- **Level 5**: Balanced speed and compression (recommended)")
        lines.append("- **Level 9**: Maximum compression, slower creation")
        lines.append("- **FUSE cache**: Increase for better random read performance")
        lines.append("- **Memory limit**: Set for large datasets to control resource usage")
        
        return '\n'.join(lines)


def main():
    parser = argparse.ArgumentParser(
        description='Generate comprehensive benchmark report'
    )
    parser.add_argument('input_json', help='Input JSON file from run_all_benchmarks.py')
    parser.add_argument('output_md', help='Output Markdown file')
    
    args = parser.parse_args()
    
    # Load results
    with open(args.input_json, 'r') as f:
        results = json.load(f)
    
    # Generate report
    generator = ComprehensiveReportGenerator(results)
    markdown = generator.generate()
    
    # Save report
    output_path = Path(args.output_md)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    with open(output_path, 'w') as f:
        f.write(markdown)
    
    print(f"Report generated: {output_path}")
    return 0


if __name__ == '__main__':
    sys.exit(main())