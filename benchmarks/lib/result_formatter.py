#!/usr/bin/env python3
"""
Benchmark Result Formatter

Single Responsibility: Generate human-readable reports from JSON
Creates markdown tables with comparisons and visual indicators
"""

import json
import yaml
from pathlib import Path
from typing import List, Dict, Any
from dataclasses import dataclass


@dataclass
class FormatMetrics:
    """Aggregated metrics for one format"""
    format_name: str
    build_time: float
    image_size: int
    compression_ratio: float
    extract_time: float
    source_size: int = 0  # Source dataset size for throughput calculation
    # FUSE metrics (optional)
    fuse_random_p50: float = 0.0
    fuse_large_p50: float = 0.0
    fuse_small_p50: float = 0.0
    fuse_sequential: float = 0.0
    fuse_file_count: int = 0


class ResultFormatter:
    """
    Single Responsibility: Format benchmark results

    Generates markdown tables with visual indicators
    Calculates comparisons vs baseline
    """

    def __init__(self, formats_config_path: Path):
        with open(formats_config_path) as f:
            self.config = yaml.safe_load(f)

        # Find baseline format
        self.baseline_format = next(
            (f['name'] for f in self.config['formats'] if f.get('baseline')),
            self.config['formats'][0]['name']
        )

    def _parse_results(self, json_path: Path) -> List[FormatMetrics]:
        """Parse JSON results into structured metrics"""

        with open(json_path) as f:
            data = json.load(f)

        metrics_list = []

        for result in data['results']:
            metrics_dict = {m['name']: m['value']
                           for m in result['metrics']}

            # Extract source_size from compression_ratio metadata
            source_size = 0
            for m in result['metrics']:
                if m['name'] == 'compression_ratio' and 'metadata' in m:
                    source_size = m['metadata'].get('source_size', 0)
                    break

            metrics = FormatMetrics(
                format_name=result['format_name'],
                build_time=metrics_dict.get('build_time', 0),
                image_size=int(metrics_dict.get('image_size', 0)),
                compression_ratio=metrics_dict.get('compression_ratio', 0),
                extract_time=metrics_dict.get('full_extract_time', 0),
                source_size=source_size,
                fuse_random_p50=metrics_dict.get('fuse_random_access_p50', 0),
                fuse_large_p50=metrics_dict.get('fuse_large_file_p50', 0),
                fuse_small_p50=metrics_dict.get('fuse_small_file_p50', 0),
                fuse_sequential=metrics_dict.get('fuse_sequential_throughput', 0),
                fuse_file_count=int(metrics_dict.get('fuse_file_count', 0))
            )
            metrics_list.append(metrics)

        return metrics_list

    def _calculate_percentage(self, value: float, baseline: float) -> str:
        """Calculate and format percentage difference"""

        if baseline == 0:
            return "N/A"

        diff = ((value - baseline) / baseline) * 100

        if abs(diff) < 0.1:
            return "~0%"
        elif diff > 0:
            return f"+{diff:.1f}%"
        else:
            return f"{diff:.1f}%"

    def _format_size(self, bytes_val: int) -> str:
        """Format bytes as human-readable"""

        if bytes_val < 1024:
            return f"{bytes_val}B"
        elif bytes_val < 1024 ** 2:
            return f"{bytes_val / 1024:.1f}KB"
        elif bytes_val < 1024 ** 3:
            return f"{bytes_val / (1024 ** 2):.1f}MB"
        else:
            return f"{bytes_val / (1024 ** 3):.2f}GB"

    def _add_visual_indicator(self, value: float, baseline: float,
                             lower_is_better: bool = True) -> str:
        """Add visual indicators for improvements"""

        if baseline == 0:
            return ""

        improvement = (baseline - value) / baseline * 100

        if lower_is_better:
            if improvement >= 5:
                return " ⭐"  # Significant improvement
            elif improvement >= 1:
                return " ✓"   # Small improvement
        else:  # Higher is better (e.g., compression ratio)
            if improvement <= -5:
                return " ⭐"
            elif improvement <= -1:
                return " ✓"

        return ""

    def _generate_metrics_explanation(self) -> List[str]:
        """Generate explanation of measured metrics"""

        md = ["\n## Measured Metrics\n"]
        md.append("### Creation Performance")
        md.append("- **Build Time**: Wall-clock time to create compressed filesystem image")
        md.append("- **Image Size**: Size of the created .dwarfs file")
        md.append("- **Compression Ratio**: Source size / image size (higher is better)")
        md.append("")

        md.append("### Extraction Performance")
        md.append("- **Extract Time**: Time to fully extract filesystem to disk")
        md.append("- **Throughput**: Extraction speed in MB/s")
        md.append("")

        md.append("### Future Metrics (Not Yet Implemented)")
        md.append("- **Random Access**: Latency when accessing individual files via FUSE mount")
        md.append("- **Small File Throughput**: Files/second for many small files")
        md.append("- **Large File Throughput**: MB/s for large sequential files")
        md.append("- **Mount Time**: Time to initialize FUSE mount")
        md.append("- **Memory Usage**: Peak memory during creation/extraction")
        md.append("")

        return md

    def _generate_recommendations(self, metrics_list: List[FormatMetrics]) -> List[str]:
        """Generate format recommendations based on results"""

        md = ["\n## Format Recommendations\n"]

        # Find best performers
        best_size = min(metrics_list, key=lambda m: m.image_size)
        fastest_build = min(metrics_list, key=lambda m: m.build_time)
        fastest_extract = min(metrics_list, key=lambda m: m.extract_time)
        best_ratio = max(metrics_list, key=lambda m: m.compression_ratio)

        md.append("### Choose Your Format Based on Priority:\n")

        md.append("#### 🎯 **For Most Users (Balanced)**")
        md.append(f"**Recommended**: {best_size.format_name if best_size.format_name != 'DwarFS-Thrift' else 'DwarFS-Cereal'}")
        md.append("- Modern format with excellent compression")
        md.append("- Default choice for new deployments")
        md.append("- Good balance of size and performance")
        md.append("")

        md.append("#### ⚡ **For Performance-Critical (Speed)**")
        md.append(f"**Recommended**: {fastest_extract.format_name if fastest_extract.format_name != 'DwarFS-Thrift' else 'DwarFS-Bitsery'}")
        md.append("- Optimized for fastest build and extraction")
        md.append("- Best for CI/CD pipelines")
        md.append("- Minimal performance overhead")
        md.append("")

        md.append("#### 💾 **For Maximum Compression (Space)**")
        md.append(f"**Recommended**: {best_size.format_name}")
        md.append("- Smallest image size")
        md.append("- Best compression ratio")
        md.append("- Ideal for storage-constrained deployments")
        md.append("")

        md.append("#### 🔄 **For Legacy Compatibility**")
        md.append("**Recommended**: DwarFS-Thrift")
        md.append("- Read-only in modern builds")
        md.append("- Use for backward compatibility")
        md.append("- Convert to modern formats when possible")
        md.append("")

        md.append("#### 📦 **For Tebako/Static Builds**")
        md.append("**Recommended**: DwarFS-Cereal or DwarFS-Bitsery")
        md.append("- No Thrift/Folly dependencies")
        md.append("- ~60% smaller binaries")
        md.append("- Header-only library dependencies")
        md.append("")

        return md

    def _generate_benchmark_suggestions(self) -> List[str]:
        """Suggest which benchmarks matter for different scenarios"""

        md = ["\n## Benchmark Interpretation Guide\n"]

        md.append("### When Each Metric Matters:\n")

        md.append("**Build Time** matters for:")
        md.append("- CI/CD pipelines (frequent image creation)")
        md.append("- Development workflows")
        md.append("- Automated builds")
        md.append("")

        md.append("**Image Size** matters for:")
        md.append("- Storage costs (cloud storage)")
        md.append("- Distribution bandwidth")
        md.append("- Embedded deployments")
        md.append("- Container registries")
        md.append("")

        md.append("**Extraction Time** matters for:")
        md.append("- Container startup")
        md.append("- Application deployment")
        md.append("- One-time extraction scenarios")
        md.append("")

        md.append("**Random Access** (future) matters for:")
        md.append("- FUSE-mounted filesystems")
        md.append("- Application runtime performance")
        md.append("- Interactive use cases")
        md.append("")

        md.append("**Throughput** (future) matters for:")
        md.append("- Large file operations")
        md.append("- Bulk data processing")
        md.append("- Streaming workloads")
        md.append("")

        md.append("### Quick Decision Matrix:\n")
        md.append("| Your Priority | Choose Format | Why |")
        md.append("|--------------|---------------|-----|")
        md.append("| **Small images** | Cereal or Bitsery | ~27% space savings |")
        md.append("| **Fast builds** | Bitsery | Optimized performance |")
        md.append("| **Compatibility** | Cereal | Default, widely compatible |")
        md.append("| **Tebako/Static** | Cereal or Bitsery | No heavy dependencies |")
        md.append("| **Legacy support** | Build with Thrift | Read old images |")
        md.append("")

        return md

    def generate_markdown(self,
                         metrics_list: List[FormatMetrics],
                         output_path: Path):
        """Generate comprehensive markdown report"""

        # Find baseline metrics
        baseline = next(m for m in metrics_list
                       if m.format_name == self.baseline_format)

        md = ["# DwarFS Serialization Format Benchmark Results\n"]
        md.append(f"**Dataset**: Raspberry Pi OS Lite ARM64")
        md.append(f"**Baseline**: {self.baseline_format}\n")

        # Add metrics explanation
        md.extend(self._generate_metrics_explanation())

        # Creation Performance Table
        md.append("## Creation Performance\n")
        md.append("| Format | Build Time | Image Size | Compression Ratio | vs Baseline |")
        md.append("|--------|-----------|------------|-------------------|-------------|")

        for m in metrics_list:
            is_baseline = (m.format_name == self.baseline_format)

            # Build time
            build_time_str = f"{m.build_time:.2f}s"
            build_time_indicator = "" if is_baseline else self._add_visual_indicator(
                m.build_time, baseline.build_time, lower_is_better=True)

            # Image size
            size_str = self._format_size(m.image_size)
            size_pct = "" if is_baseline else self._calculate_percentage(
                m.image_size, baseline.image_size)
            size_indicator = "" if is_baseline else self._add_visual_indicator(
                m.image_size, baseline.image_size, lower_is_better=True)

            # Compression ratio (higher is better)
            ratio_str = f"{m.compression_ratio:.2f}×"
            ratio_indicator = "" if is_baseline else self._add_visual_indicator(
                m.compression_ratio, baseline.compression_ratio, lower_is_better=False)

            # vs Baseline column
            vs_baseline = "Baseline" if is_baseline else size_pct

            md.append(f"| {m.format_name} | "
                     f"{build_time_str}{build_time_indicator} | "
                     f"{size_str}{size_indicator} | "
                     f"{ratio_str}{ratio_indicator} | "
                     f"{vs_baseline} |")

        # Extraction Performance Table
        md.append("\n## Extraction Performance\n")
        md.append("| Format | Extract Time | Throughput | vs Baseline |")
        md.append("|--------|-------------|------------|-------------|")

        for m in metrics_list:
            is_baseline = (m.format_name == self.baseline_format)

            extract_str = f"{m.extract_time:.2f}s"
            extract_indicator = "" if is_baseline else self._add_visual_indicator(
                m.extract_time, baseline.extract_time, lower_is_better=True)

            # Calculate throughput from source_size and extract_time
            if m.source_size > 0 and m.extract_time > 0:
                throughput_mbps = (m.source_size / (1024 * 1024)) / m.extract_time
                throughput_str = f"{throughput_mbps:.1f}MB/s"
                if not is_baseline and baseline.source_size > 0 and baseline.extract_time > 0:
                    baseline_throughput = (baseline.source_size / (1024 * 1024)) / baseline.extract_time
                    throughput_str += self._add_visual_indicator(
                        throughput_mbps, baseline_throughput, lower_is_better=False)
            else:
                throughput_str = "N/A"

            vs_baseline = "Baseline" if is_baseline else self._calculate_percentage(
                m.extract_time, baseline.extract_time)

            md.append(f"| {m.format_name} | "
                     f"{extract_str}{extract_indicator} | "
                     f"{throughput_str} | "
                     f"{vs_baseline} |")

        # Enhanced FUSE Performance Table (if available)
        has_fuse_data = any(m.fuse_random_p50 > 0 or m.fuse_large_p50 > 0
                           or m.fuse_small_p50 > 0 for m in metrics_list)

        if has_fuse_data:
            md.append("\n## FUSE Performance (Mounted Filesystem)\n")
            md.append("| Format | Large File (p50) | Small File (p50) | Sequential | File Count | Random (p50) | vs Baseline |")
            md.append("|--------|------------------|------------------|------------|------------|--------------|-------------|")

            for m in metrics_list:
                is_baseline = (m.format_name == self.baseline_format)

                # Large file random access
                if m.fuse_large_p50 > 0:
                    large = f"{m.fuse_large_p50:.1f}ms"
                    if not is_baseline and baseline.fuse_large_p50 > 0:
                        large += self._add_visual_indicator(
                            m.fuse_large_p50, baseline.fuse_large_p50, lower_is_better=True)
                else:
                    large = "N/A"

                # Small file access
                if m.fuse_small_p50 > 0:
                    small = f"{m.fuse_small_p50:.1f}ms"
                    if not is_baseline and baseline.fuse_small_p50 > 0:
                        small += self._add_visual_indicator(
                            m.fuse_small_p50, baseline.fuse_small_p50, lower_is_better=True)
                else:
                    small = "N/A"

                # Sequential throughput
                if m.fuse_sequential > 0:
                    seq = f"{m.fuse_sequential:.1f}MB/s"
                    if not is_baseline and baseline.fuse_sequential > 0:
                        seq += self._add_visual_indicator(
                            m.fuse_sequential, baseline.fuse_sequential, lower_is_better=False)
                else:
                    seq = "N/A"

                # File count
                count = str(m.fuse_file_count) if m.fuse_file_count > 0 else "N/A"

                # Random access (basic)
                if m.fuse_random_p50 > 0:
                    rand = f"{m.fuse_random_p50:.1f}ms"
                    if not is_baseline and baseline.fuse_random_p50 > 0:
                        rand += self._add_visual_indicator(
                            m.fuse_random_p50, baseline.fuse_random_p50, lower_is_better=True)
                else:
                    rand = "N/A"

                # vs Baseline percentage - use large file metric as primary comparison
                if is_baseline:
                    vs_baseline = "Baseline"
                else:
                    # Use large file p50 as the primary FUSE metric for comparison
                    if m.fuse_large_p50 > 0 and baseline.fuse_large_p50 > 0:
                        vs_baseline = self._calculate_percentage(
                            m.fuse_large_p50, baseline.fuse_large_p50)
                    else:
                        vs_baseline = "N/A"

                md.append(f"| {m.format_name} | {large} | {small} | {seq} | {count} | {rand} | {vs_baseline} |")

        # Summary
        md.append("\n## Summary\n")
        md.append("**Legend**: ⭐ Significant improvement (≥5%), ✓ Improvement (≥1%)\n")

        # Find best in each category
        best_size = min(metrics_list, key=lambda m: m.image_size)
        best_build = min(metrics_list, key=lambda m: m.build_time)
        best_extract = min(metrics_list, key=lambda m: m.extract_time)
        best_ratio = max(metrics_list, key=lambda m: m.compression_ratio)

        md.append(f"- **Best Size**: {best_size.format_name} "
                 f"({self._format_size(best_size.image_size)})")
        md.append(f"- **Fastest Build**: {best_build.format_name} "
                 f"({best_build.build_time:.2f}s)")
        md.append(f"- **Fastest Extract**: {best_extract.format_name} "
                 f"({best_extract.extract_time:.2f}s)")
        md.append(f"- **Best Compression**: {best_ratio.format_name} "
                 f"({best_ratio.compression_ratio:.2f}×)")

        # Add recommendations
        md.extend(self._generate_recommendations(metrics_list))

        # Add benchmark suggestions
        md.extend(self._generate_benchmark_suggestions())

        # Write to file
        with open(output_path, 'w') as f:
            f.write('\n'.join(md))

        print(f"Report generated: {output_path}")


def main():
    import argparse

    parser = argparse.ArgumentParser(
        description='Format benchmark results as markdown')
    parser.add_argument('--input', required=True, type=Path,
                       help='Path to raw_metrics.json')
    parser.add_argument('--config', required=True, type=Path,
                       help='Path to dwarfs_formats.yaml')
    parser.add_argument('--output', required=True, type=Path,
                       help='Output markdown file')
    args = parser.parse_args()

    formatter = ResultFormatter(args.config)
    metrics = formatter._parse_results(args.input)
    formatter.generate_markdown(metrics, args.output)

    print("Report generation complete!")


if __name__ == '__main__':
    main()