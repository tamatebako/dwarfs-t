#!/usr/bin/env python3
"""
Generate Markdown benchmark report from JSON data.

Usage:
    python3 generate_metadata_report.py input.json output.md
"""

import argparse
import json
import sys
from pathlib import Path
from typing import Dict, Any

def format_bytes(bytes_val: int) -> str:
    """Format bytes in human-readable form."""
    for unit in ['B', 'KiB', 'MiB', 'GiB']:
        if bytes_val < 1024.0:
            return f"{bytes_val:.1f} {unit}"
        bytes_val /= 1024.0
    return f"{bytes_val:.1f} TiB"

def format_percent(ratio: float) -> str:
    """Format ratio as percentage with sign."""
    percent = (ratio - 1.0) * 100
    sign = '+' if percent > 0 else ''
    return f"{sign}{percent:.2f}%"

def generate_header(data: Dict[str, Any]) -> str:
    """Generate report header section."""
    md = []
    md.append("# DwarFS Metadata Format Benchmark Report")
    md.append("")
    md.append(f"**Date**: {data['metadata']['date']}")
    md.append(f"**Dataset**: {data['metadata']['dataset']['path']}")
    md.append(f"**Files**: {data['metadata']['dataset']['files']}")
    md.append(f"**Dataset Size**: {format_bytes(data['metadata']['dataset']['size_bytes'])}")
    md.append(f"**Iterations**: {data['metadata']['iterations']}")
    md.append("")
    return "\n".join(md)

def generate_summary(data: Dict[str, Any]) -> str:
    """Generate summary table."""
    md = []
    md.append("## Executive Summary")
    md.append("")
    
    fb = data['results']['flatbuffers']
    tb = data['results']['thrift']
    comp = data['comparison']
    
    md.append("| Metric | FlatBuffers | Thrift | Ratio |")
    md.append("|--------|-------------|--------|-------|")
    
    # Image size
    md.append(f"| **Image Size** | {format_bytes(fb['image_size_bytes'])} "
              f"({fb['image_size_bytes']:,} bytes) | "
              f"{format_bytes(tb['image_size_bytes'])} "
              f"({tb['image_size_bytes']:,} bytes) | "
              f"**{comp['size_ratio']:.4f}x** ({format_percent(comp['size_ratio'])}) |")
    
    # Creation time
    md.append(f"| **Creation Time** (mean) | {fb['real_mean']:.3f}s | "
              f"{tb['real_mean']:.3f}s | "
              f"{comp['time_ratio']:.2f}x |")
    
    # Creation time (median)
    md.append(f"| **Creation Time** (median) | {fb['real_median']:.3f}s | "
              f"{tb['real_median']:.3f}s | "
              f"{fb['real_median']/tb['real_median']:.2f}x |")
    
    md.append("")
    return "\n".join(md)

def generate_verdict(data: Dict[str, Any]) -> str:
    """Generate verdict section."""
    md = []
    md.append("## Verdict")
    md.append("")
    
    comp = data['comparison']
    size_ratio = comp['size_ratio']
    
    if size_ratio <= 1.10:
        verdict = "✅ **EXCELLENT**"
        explanation = f"FlatBuffers overhead is only {format_percent(size_ratio)}, well within the ≤110% target."
    elif size_ratio <= 1.15:
        verdict = "✅ **ACCEPTABLE**"
        explanation = f"FlatBuffers overhead is {format_percent(size_ratio)}, slightly above target but acceptable."
    else:
        verdict = "⚠️ **REVIEW NEEDED**"
        explanation = f"FlatBuffers overhead is {format_percent(size_ratio)}, significantly above the ≤110% target."
    
    md.append(f"**Size Efficiency**: {verdict}")
    md.append("")
    md.append(explanation)
    md.append("")
    
    return "\n".join(md)

def generate_detailed_analysis(data: Dict[str, Any]) -> str:
    """Generate detailed statistical analysis."""
    md = []
    md.append("## Detailed Analysis")
    md.append("")
    
    fb = data['results']['flatbuffers']
    tb = data['results']['thrift']
    
    # Size analysis
    md.append("### Image Size Comparison")
    md.append("")
    md.append("| Format | Size (bytes) | Size (human) | Overhead |")
    md.append("|--------|--------------|--------------|----------|")
    md.append(f"| **FlatBuffers** | {fb['image_size_bytes']:,} | "
              f"{format_bytes(fb['image_size_bytes'])} | "
              f"{format_percent(data['comparison']['size_ratio'])} |")
    md.append(f"| **Thrift** | {tb['image_size_bytes']:,} | "
              f"{format_bytes(tb['image_size_bytes'])} | baseline |")
    md.append("")
    
    # Timing analysis
    md.append("### Creation Time Statistics")
    md.append("")
    md.append("| Metric | FlatBuffers | Thrift |")
    md.append("|--------|-------------|--------|")
    md.append(f"| **Mean** | {fb['real_mean']:.3f}s | {tb['real_mean']:.3f}s |")
    md.append(f"| **Median** | {fb['real_median']:.3f}s | {tb['real_median']:.3f}s |")
    md.append(f"| **Std Dev** | {fb['real_stddev']:.3f}s | {tb['real_stddev']:.3f}s |")
    md.append(f"| **Min** | {min(fb['real_samples']):.3f}s | {min(tb['real_samples']):.3f}s |")
    md.append(f"| **Max** | {max(fb['real_samples']):.3f}s | {max(tb['real_samples']):.3f}s |")
    md.append("")
    
    # Sample distribution
    md.append("### Raw Samples")
    md.append("")
    md.append("**FlatBuffers**:")
    for i, sample in enumerate(fb['real_samples'], 1):
        md.append(f"- Run {i}: {sample:.3f}s")
    md.append("")
    
    md.append("**Thrift**:")
    for i, sample in enumerate(tb['real_samples'], 1):
        md.append(f"- Run {i}: {sample:.3f}s")
    md.append("")
    
    return "\n".join(md)

def generate_recommendations(data: Dict[str, Any]) -> str:
    """Generate recommendations section."""
    md = []
    md.append("## Recommendations")
    md.append("")
    
    comp = data['comparison']
    
    md.append("### For New Projects")
    md.append("")
    md.append("1. **Use FlatBuffers** as the default metadata format")
    md.append(f"   - Size overhead: Only {format_percent(comp['size_ratio'])}")
    md.append("   - Portability: Excellent (header-only, works everywhere)")
    md.append("   - Dependencies: Minimal (no Folly/fbthrift)")
    md.append("")
    
    md.append("### For Legacy Support")
    md.append("")
    md.append("1. **Enable Thrift** only if needed for reading old images")
    md.append("2. **Consider migration**: Convert old Thrift images to FlatBuffers")
    md.append("3. **Build configuration**: Use dual-format builds only when necessary")
    md.append("")
    
    md.append("### Build Configuration")
    md.append("")
    md.append("```cmake")
    md.append("# Recommended: FlatBuffers-only")
    md.append("-DDWARFS_WITH_FLATBUFFERS=ON")
    md.append("-DDWARFS_WITH_THRIFT=OFF")
    md.append("")
    md.append("# For legacy support: Dual-format")
    md.append("-DDWARFS_WITH_FLATBUFFERS=ON")
    md.append("-DDWARFS_WITH_THRIFT=ON")
    md.append("```")
    md.append("")
    
    return "\n".join(md)

def generate_appendix(data: Dict[str, Any]) -> str:
    """Generate appendix with build information."""
    md = []
    md.append("## Appendix")
    md.append("")
    
    md.append("### Build Information")
    md.append("")
    md.append("| Component | Path |")
    md.append("|-----------|------|")
    md.append(f"| **FlatBuffers mkdwarfs** | `{data['metadata']['builds']['flatbuffers']}` |")
    md.append(f"| **Thrift mkdwarfs** | `{data['metadata']['builds']['thrift']}` |")
    md.append("")
    
    md.append("### Test Command")
    md.append("")
    md.append("```bash")
    md.append("python3 benchmarks/metadata_format_benchmark.py \\")
    md.append(f"  --flatbuffers-mkdwarfs {data['metadata']['builds']['flatbuffers']} \\")
    md.append(f"  --thrift-mkdwarfs {data['metadata']['builds']['thrift']} \\")
    md.append(f"  --dataset {data['metadata']['dataset']['path']} \\")
    md.append(f"  --iterations {data['metadata']['iterations']} \\")
    md.append("  --output results.json")
    md.append("```")
    md.append("")
    
    return "\n".join(md)

def generate_report(data: Dict[str, Any]) -> str:
    """Generate complete Markdown report."""
    
    sections = [
        generate_header(data),
        generate_summary(data),
        generate_verdict(data),
        generate_detailed_analysis(data),
        generate_recommendations(data),
        generate_appendix(data)
    ]
    
    return "\n".join(sections)

def main():
    parser = argparse.ArgumentParser(
        description='Generate benchmark report from JSON data',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__
    )
    parser.add_argument('input', type=Path, help='Input JSON file')
    parser.add_argument('output', type=Path, help='Output Markdown file')
    
    args = parser.parse_args()
    
    # Read JSON
    try:
        with open(args.input) as f:
            data = json.load(f)
    except FileNotFoundError:
        print(f"Error: Input file not found: {args.input}", file=sys.stderr)
        return 1
    except json.JSONDecodeError as e:
        print(f"Error: Invalid JSON in {args.input}: {e}", file=sys.stderr)
        return 1
    
    # Generate report
    report = generate_report(data)
    
    # Write output
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with open(args.output, 'w') as f:
        f.write(report)
    
    print(f"✅ Report written to {args.output}", file=sys.stderr)
    
    return 0

if __name__ == '__main__':
    sys.exit(main())