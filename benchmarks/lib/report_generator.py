#!/usr/bin/env python3
"""
DwarFS Metadata Format Benchmark Report Generator

Generates comprehensive Markdown report from JSON benchmark results
comparing Thrift, Cereal, and Bitsery metadata formats.
"""

import json
import sys
from dataclasses import dataclass
from datetime import datetime
from typing import Dict, List, Any, Optional


@dataclass
class LatencyMetric:
    """Value object for latency metric"""
    samples: int
    avg_us: float
    p50_us: float
    p90_us: float
    p99_us: float


@dataclass
class FUSEMetrics:
    """Value object for FUSE metrics"""
    mount_time_seconds: float
    init_time_seconds: float
    read_single: Dict[str, Any]
    read_all: Dict[str, Any]
    latency: Dict[str, LatencyMetric]
    std_dev_mount: float = 0.0
    std_dev_init: float = 0.0

    @staticmethod
    def parse_latency(latency_data: Optional[Dict]) -> Dict[str, LatencyMetric]:
        """Parse latency data into LatencyMetric objects"""
        if not latency_data:
            return {}

        return {
            op: LatencyMetric(**data)
            for op, data in latency_data.items()
        }


@dataclass
class FormatResult:
    """Value object for format benchmark result"""
    dataset: str
    format: str
    compression: Dict[str, Any]
    extract_all: Dict[str, Any]
    extract_single: Dict[str, Any]
    fuse: FUSEMetrics

    @property
    def compression_time(self) -> float:
        return self.compression['time_seconds']

    @property
    def compression_size(self) -> int:
        return self.compression['size_bytes']

    @property
    def compression_memory(self) -> float:
        return self.compression['memory_mb']


class MetadataFormatReport:
    """Main report generator"""

    FORMATS = ['thrift', 'flatbuffers']

    def __init__(self, json_path: str):
        with open(json_path, 'r') as f:
            self.data = json.load(f)

        self.metadata = self.data['metadata']
        self.datasets = self.data['datasets']
        self.results = self._parse_results()

    def _parse_results(self) -> List[FormatResult]:
        """Parse JSON results into FormatResult objects"""
        results = []
        for r in self.data['results']:
            fuse_data = r['fuse'].copy()
            fuse_data['latency'] = FUSEMetrics.parse_latency(fuse_data.get('latency'))

            results.append(FormatResult(
                dataset=r['dataset'],
                format=r['format'],
                compression=r['compression'],
                extract_all=r['extract_all'],
                extract_single=r['extract_single'],
                fuse=FUSEMetrics(**fuse_data)
            ))
        return results

    def generate(self) -> str:
        """Generate complete Markdown report"""
        output = []
        output.append(self._generate_header())
        output.append(self._generate_system_info())
        output.append(self._generate_executive_summary())
        output.append('')

        for dataset in self.datasets.keys():
            output.append(self._generate_dataset_section(dataset))
            output.append('')

        output.append(self._generate_analysis())
        output.append(self._generate_recommendations())

        return '\n'.join(output)

    def _generate_header(self) -> str:
        """Generate report header"""
        timestamp = datetime.fromisoformat(self.metadata['timestamp'].replace('Z', '+00:00'))
        timestamp_str = timestamp.strftime('%Y-%m-%d %H:%M:%S UTC')

        return f"""# DwarFS Metadata Format Benchmark Report

**Generated:** {timestamp_str}

**Platform:** {self.metadata['os']}, {self.metadata['cpu']}, {self.metadata['memory']}

**DwarFS:** {self.metadata['dwarfs_version']}

**FUSE:** {self.metadata['fuse_implementation']}"""

    def _generate_system_info(self) -> str:
        """Generate system information table"""
        return f"""## System Information

| Property | Value |
|----------|-------|
| Hostname | {self.metadata['hostname']} |
| OS | {self.metadata['os']} |
| CPU | {self.metadata['cpu']} |
| Memory | {self.metadata['memory']} |
| DwarFS Version | {self.metadata['dwarfs_version']} |
| FUSE Implementation | {self.metadata['fuse_implementation']} |"""

    def _generate_executive_summary(self) -> str:
        """Generate executive summary with best performers"""
        summary = self._find_best_performers()

        return f"""## Executive Summary

### Best Performers

| Metric | Winner | Runner-up | Note |
|--------|--------|-----------|------|
| Smallest Archive | {summary['smallest']['format']} | {summary['smallest']['runner_up']} | {self._format_bytes(summary['smallest']['value'])} |
| Fastest Compression | {summary['fastest_compress']['format']} | {summary['fastest_compress']['runner_up']} | {self._format_time(summary['fastest_compress']['value'])} |
| Fastest Extraction | {summary['fastest_extract']['format']} | {summary['fastest_extract']['runner_up']} | {round(summary['fastest_extract']['value'], 1)} MB/s |
| Fastest FUSE Read | {summary['fastest_fuse']['format']} | {summary['fastest_fuse']['runner_up']} | {round(summary['fastest_fuse']['value'], 1)} MB/s |
| Lowest FUSE Latency | {summary['lowest_latency']['format']} | {summary['lowest_latency']['runner_up']} | {round(summary['lowest_latency']['value'], 1)} µs avg |"""

    def _find_best_performers(self) -> Dict[str, Dict[str, Any]]:
        """Find best performers across all metrics"""
        # Aggregate across all datasets
        all_results = self.results

        # Find smallest archive (lowest size)
        smallest = self._find_best(
            all_results,
            lambda r: r.compression_size,
            reverse=False
        )

        # Find fastest compression (lowest time)
        fastest_compress = self._find_best(
            all_results,
            lambda r: r.compression_time,
            reverse=False
        )

        # Find fastest extraction (highest throughput)
        fastest_extract = self._find_best(
            all_results,
            lambda r: r.extract_all['throughput_mbs']
        )

        # Find fastest FUSE read (highest throughput)
        fastest_fuse = self._find_best(
            all_results,
            lambda r: r.fuse.read_all['throughput_mbs']
        )

        # Find lowest FUSE latency (lowest avg)
        def avg_latency(r):
            latencies = [m.avg_us for m in r.fuse.latency.values()]
            return sum(latencies) / len(latencies) if latencies else float('inf')

        lowest_latency = self._find_best(
            all_results,
            avg_latency,
            reverse=False
        )

        return {
            'smallest': smallest,
            'fastest_compress': fastest_compress,
            'fastest_extract': fastest_extract,
            'fastest_fuse': fastest_fuse,
            'lowest_latency': lowest_latency
        }

    def _find_best(self, results: List[FormatResult], value_func, reverse: bool = True) -> Dict[str, Any]:
        """Find best performer for a given metric"""
        values = [(r.format, value_func(r)) for r in results]
        sorted_values = sorted(values, key=lambda x: x[1], reverse=reverse)

        return {
            'format': sorted_values[0][0],
            'value': sorted_values[0][1],
            'runner_up': sorted_values[1][0] if len(sorted_values) > 1 else 'N/A'
        }

    def _generate_dataset_section(self, dataset: str) -> str:
        """Generate complete section for a dataset"""
        dataset_results = [r for r in self.results if r.dataset == dataset]
        dataset_info = self.datasets[dataset]

        sections = [
            f"## {dataset.capitalize()} Results",
            '',
            self._generate_dataset_info_table(dataset_info),
            '',
            self._generate_compression_table(dataset_results),
            '',
            self._generate_extraction_tables(dataset_results),
            '',
            self._generate_fuse_tables(dataset_results)
        ]

        return '\n'.join(sections)

    def _generate_dataset_info_table(self, info: Dict[str, Any]) -> str:
        """Generate dataset information table"""
        file_count_formatted = f"{info['file_count']:,}"

        return f"""### Dataset Information

| Property | Value |
|----------|-------|
| Source | `{info['source_path']}` |
| Size | {self._format_bytes(info['source_size_bytes'])} |
| Files | {file_count_formatted} |
| Test File | `{info['test_file']}` |"""

    def _generate_compression_table(self, results: List[FormatResult]) -> str:
        """Generate compression performance table"""
        baseline = next((r for r in results if r.format == 'thrift'), None)

        rows = []
        for fmt in self.FORMATS:
            result = next((r for r in results if r.format == fmt), None)
            if not result:
                continue

            time_diff = self._calc_diff(
                result.compression_time,
                baseline.compression_time if baseline else None,
                lower_is_better=True
            )
            size_diff = self._calc_diff(
                result.compression_size,
                baseline.compression_size if baseline else None,
                lower_is_better=True
            )
            mem_diff = self._calc_diff(
                result.compression_memory,
                baseline.compression_memory if baseline else None,
                lower_is_better=True
            )

            rows.append(
                f"| {fmt.capitalize()} | {self._format_time(result.compression_time)} | {time_diff} | "
                f"{self._format_bytes(result.compression_size)} | {size_diff} | "
                f"{round(result.compression_memory, 1)} MB | {mem_diff} |"
            )

        return f"""### Compression Performance

| Format | Time | vs Baseline | Size | vs Baseline | Memory | vs Baseline |
|--------|------|-------------|------|-------------|--------|-------------|
{chr(10).join(rows)}"""

    def _generate_extraction_tables(self, results: List[FormatResult]) -> str:
        """Generate extraction performance tables"""
        sections = [
            '### Extraction Performance',
            '',
            '#### Extract All Files',
            '',
            self._generate_extract_all_table(results),
            '',
            '#### Extract Single File',
            '',
            self._generate_extract_single_table(results)
        ]

        return '\n'.join(sections)

    def _generate_extract_all_table(self, results: List[FormatResult]) -> str:
        """Generate extract all files table"""
        baseline = next((r for r in results if r.format == 'thrift'), None)

        rows = []
        for fmt in self.FORMATS:
            result = next((r for r in results if r.format == fmt), None)
            if not result:
                continue

            extract = result.extract_all
            time_diff = self._calc_diff(
                extract['time_seconds'],
                baseline.extract_all['time_seconds'] if baseline else None,
                lower_is_better=True
            )
            tp_diff = self._calc_diff(
                extract['throughput_mbs'],
                baseline.extract_all['throughput_mbs'] if baseline else None,
                lower_is_better=False
            )

            rows.append(
                f"| {fmt.capitalize()} | {self._format_time(extract['time_seconds'])} | {time_diff} | "
                f"{round(extract['throughput_mbs'], 1)} MB/s | {tp_diff} | "
                f"{round(extract['memory_mb'], 1)} MB |"
            )

        return f"""| Format | Time | vs Baseline | Throughput | vs Baseline | Memory |
|--------|------|-------------|------------|-------------|--------|
{chr(10).join(rows)}"""

    def _generate_extract_single_table(self, results: List[FormatResult]) -> str:
        """Generate extract single file table"""
        baseline = next((r for r in results if r.format == 'thrift'), None)
        test_file = results[0].extract_single['file']

        rows = []
        for fmt in self.FORMATS:
            result = next((r for r in results if r.format == fmt), None)
            if not result:
                continue

            extract = result.extract_single
            time_diff = self._calc_diff(
                extract['time_seconds'],
                baseline.extract_single['time_seconds'] if baseline else None,
                lower_is_better=True
            )
            tp_diff = self._calc_diff(
                extract['throughput_mbs'],
                baseline.extract_single['throughput_mbs'] if baseline else None,
                lower_is_better=False
            )

            rows.append(
                f"| {fmt.capitalize()} | {self._format_time(extract['time_seconds'])} | {time_diff} | "
                f"{round(extract['throughput_mbs'], 1)} MB/s | {tp_diff} | "
                f"{round(extract['memory_mb'], 1)} MB |"
            )

        return f"""**File:** `{test_file}`

| Format | Time | vs Baseline | Throughput | vs Baseline | Memory |
|--------|------|-------------|------------|-------------|--------|
{chr(10).join(rows)}"""

    def _generate_fuse_tables(self, results: List[FormatResult]) -> str:
        """Generate FUSE performance tables"""
        sections = [
            '### FUSE Performance',
            '',
            '#### Mount Time',
            '',
            self._generate_mount_time_table(results),
            '',
            '#### Read Single File',
            '',
            self._generate_fuse_read_single_table(results),
            '',
            '#### Read All Files',
            '',
            self._generate_fuse_read_all_table(results),
            '',
            '#### FUSE Operation Latency',
            '',
            self._generate_fuse_latency_table(results)
        ]

        return '\n'.join(sections)

    def _generate_mount_time_table(self, results: List[FormatResult]) -> str:
        """Generate mount time table"""
        baseline = next((r for r in results if r.format == 'thrift'), None)

        rows = []
        for fmt in self.FORMATS:
            result = next((r for r in results if r.format == fmt), None)
            if not result:
                continue

            mount_diff = self._calc_diff(
                result.fuse.mount_time_seconds,
                baseline.fuse.mount_time_seconds if baseline else None,
                lower_is_better=True
            )
            init_diff = self._calc_diff(
                result.fuse.init_time_seconds,
                baseline.fuse.init_time_seconds if baseline else None,
                lower_is_better=True
            )
            total = result.fuse.mount_time_seconds + result.fuse.init_time_seconds

            rows.append(
                f"| {fmt.capitalize()} | {self._format_time(result.fuse.mount_time_seconds)} | {mount_diff} | "
                f"{self._format_time(result.fuse.init_time_seconds)} | {init_diff} | "
                f"{self._format_time(total)} |"
            )

        return f"""| Format | Mount | vs Baseline | Init | vs Baseline | Total |
|--------|-------|-------------|------|-------------|-------|
{chr(10).join(rows)}"""

    def _generate_fuse_read_single_table(self, results: List[FormatResult]) -> str:
        """Generate FUSE read single file table"""
        baseline = next((r for r in results if r.format == 'thrift'), None)
        test_file = results[0].fuse.read_single['file']

        rows = []
        for fmt in self.FORMATS:
            result = next((r for r in results if r.format == fmt), None)
            if not result:
                continue

            read = result.fuse.read_single
            time_diff = self._calc_diff(
                read['time_seconds'],
                baseline.fuse.read_single['time_seconds'] if baseline else None,
                lower_is_better=True
            )
            tp_diff = self._calc_diff(
                read['throughput_mbs'],
                baseline.fuse.read_single['throughput_mbs'] if baseline else None,
                lower_is_better=False
            )

            rows.append(
                f"| {fmt.capitalize()} | {self._format_time(read['time_seconds'])} | {time_diff} | "
                f"{round(read['throughput_mbs'], 1)} MB/s | {tp_diff} |"
            )

        return f"""**File:** `{test_file}`

| Format | Time | vs Baseline | Throughput | vs Baseline |
|--------|------|-------------|------------|-------------|
{chr(10).join(rows)}"""

    def _generate_fuse_read_all_table(self, results: List[FormatResult]) -> str:
        """Generate FUSE read all files table"""
        baseline = next((r for r in results if r.format == 'thrift'), None)

        rows = []
        for fmt in self.FORMATS:
            result = next((r for r in results if r.format == fmt), None)
            if not result:
                continue

            read = result.fuse.read_all
            time_diff = self._calc_diff(
                read['time_seconds'],
                baseline.fuse.read_all['time_seconds'] if baseline else None,
                lower_is_better=True
            )
            tp_diff = self._calc_diff(
                read['throughput_mbs'],
                baseline.fuse.read_all['throughput_mbs'] if baseline else None,
                lower_is_better=False
            )

            rows.append(
                f"| {fmt.capitalize()} | {self._format_time(read['time_seconds'])} | {time_diff} | "
                f"{round(read['throughput_mbs'], 1)} MB/s | {tp_diff} | "
                f"{round(read['memory_mb'], 1)} MB |"
            )

        return f"""| Format | Time | vs Baseline | Throughput | vs Baseline | Memory |
|--------|------|-------------|------------|-------------|--------|
{chr(10).join(rows)}"""

    def _generate_fuse_latency_table(self, results: List[FormatResult]) -> str:
        """Generate FUSE operation latency table"""
        # Get all operation names from first result
        all_ops = sorted(results[0].fuse.latency.keys()) if results[0].fuse.latency else []
        if not all_ops:
            return ''

        rows = []
        for fmt in self.FORMATS:
            result = next((r for r in results if r.format == fmt), None)
            if not result:
                continue

            cells = []
            for op in all_ops:
                metric = result.fuse.latency.get(op)
                cells.append(str(round(metric.avg_us, 1)) if metric else 'N/A')

            rows.append(f"| {fmt.capitalize()} | {' | '.join(cells)} |")

        op_headers = ' | '.join(op.replace('op_', '') for op in all_ops)
        separator = '|' + '|'.join(['--------'] * (len(all_ops) + 1)) + '|'

        return f"""**Average Latency (µs)**

| Format | {op_headers} |
{separator}
{chr(10).join(rows)}"""

    def _generate_analysis(self) -> str:
        """Generate analysis section"""
        return """## Analysis

### Compression

The compression phase shows the trade-offs between different metadata formats:

- **Archive Size**: Measures the space efficiency of each format
- **Compression Time**: Indicates the overhead of serializing metadata
- **Memory Usage**: Shows the memory footprint during compression

### Extraction

Extraction performance reveals how efficiently each format can deserialize metadata:

- **Full Extraction**: Tests bulk operations and overall throughput
- **Single File**: Measures random access efficiency

### FUSE Performance

FUSE benchmarks demonstrate real-world usage patterns:

- **Mount Time**: How quickly the filesystem becomes available
- **Read Performance**: Throughput for accessing files via FUSE
- **Operation Latency**: Low-level FUSE operation performance"""

    def _generate_recommendations(self) -> str:
        """Generate recommendations section"""
        summary = self._find_best_performers()

        return f"""## Recommendations

Based on the benchmark results:

### For Storage Optimization
**Use {summary['smallest']['format'].capitalize()}** if minimizing archive size is the priority.

### For Compression Speed
**Use {summary['fastest_compress']['format'].capitalize()}** if fast archive creation is important.

### For Read-Heavy Workloads
**Use {summary['fastest_fuse']['format'].capitalize()}** for best FUSE read performance and lower latency.

### For Balanced Use
If you need a balance between compression and performance, evaluate the trade-offs
based on your specific use case:

- Archives mainly stored and rarely accessed: Prioritize compression ratio
- Archives frequently mounted and read: Prioritize FUSE performance
- Mixed workload: Consider the format with best overall performance across metrics

### Migration Considerations

When migrating between formats:
1. Recreate archives with the new format using `mkdwarfs --metadata-format <format>`
2. Test performance in your specific environment
3. Verify compatibility with your FUSE implementation"""

    # Helper methods

    def _calc_diff(self, value: float, baseline: Optional[float], lower_is_better: bool = True) -> str:
        """Calculate and format difference from baseline"""
        if baseline is None or baseline == 0:
            return ''

        diff = value - baseline
        relative_diff = (abs(diff) / abs(baseline)) * 100

        # Detect effectively identical values (within 0.01% or absolute diff < 0.01)
        if relative_diff < 0.01 or abs(diff) < 0.01:
            return '~'

        pct = (diff / baseline) * 100

        # Format percentage with sign
        sign = '+' if pct > 0 else ''
        pct_str = f"{sign}{round(pct, 1)}%"

        # Determine emoji based on metric type
        if lower_is_better:
            # For time/size/memory: decrease is good
            emoji = '✅' if pct < 0 else '⬆️'
        else:
            # For throughput: increase is good
            emoji = '✅' if pct > 0 else '⬆️'

        return f"{emoji} {pct_str}"

    def _format_time(self, seconds: float) -> str:
        """Format time value with appropriate unit"""
        if seconds < 0.001:
            return f"{round(seconds * 1_000_000, 1)}µs"
        elif seconds < 1:
            return f"{round(seconds * 1000, 1)}ms"
        else:
            return f"{round(seconds, 2)}s"

    def _format_bytes(self, bytes_val: int) -> str:
        """Format byte value with appropriate unit"""
        units = ['B', 'KB', 'MB', 'GB', 'TB']
        unit_index = 0
        size = float(bytes_val)

        while size >= 1024 and unit_index < len(units) - 1:
            size /= 1024
            unit_index += 1

        return f"{round(size, 2)} {units[unit_index]}"


def main():
    """Main entry point"""
    if len(sys.argv) != 3:
        print('Usage: generate_metadata_report.py <results.json> <output.md>')
        sys.exit(1)

    json_path = sys.argv[1]
    output_path = sys.argv[2]

    try:
        report = MetadataFormatReport(json_path)
        markdown = report.generate()

        with open(output_path, 'w') as f:
            f.write(markdown)

        print(f"Report generated: {output_path}")
    except FileNotFoundError:
        print(f"ERROR: JSON file not found: {json_path}")
        sys.exit(1)
    except Exception as e:
        print(f"ERROR: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)


if __name__ == '__main__':
    main()