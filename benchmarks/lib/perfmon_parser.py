"""
Perfmon Parser Module

Parse FUSE performance monitoring (perfmon) xattr output into latency metrics.
"""

import re
from dataclasses import dataclass
from typing import Dict


@dataclass
class LatencyMetric:
    """FUSE operation latency statistics"""
    samples: int
    avg_us: float
    p50_us: float
    p90_us: float
    p99_us: float


class PerfmonParser:
    """Parse FUSE perfmon output into latency metrics"""

    @staticmethod
    def parse(perfmon_text: str) -> Dict[str, LatencyMetric]:
        """
        Parse perfmon xattr output into latency metrics

        Expected format:
        [fuse.op_name]
              samples: N
              overall: X.Xs
          avg latency: X.Xus
          p50 latency: X.Xus
          p90 latency: X.Xus
          p99 latency: X.Xus
        """
        latency_metrics = {}
        current_op = None
        current_data = {}

        for line in perfmon_text.split('\n'):
            line = line.strip()
            if not line:
                continue

            # Match operation header: [fuse.op_name]
            op_match = re.match(r'\[fuse\.op_(\w+)\]', line)
            if op_match:
                # Save previous operation if exists
                if current_op and len(current_data) >= 4:
                    latency_metrics[current_op] = LatencyMetric(
                        samples=current_data.get('samples', 0),
                        avg_us=current_data.get('avg', 0.0),
                        p50_us=current_data.get('p50', 0.0),
                        p90_us=current_data.get('p90', 0.0),
                        p99_us=current_data.get('p99', 0.0)
                    )
                current_op = op_match.group(1)
                current_data = {}
                continue

            # Match samples
            samples_match = re.match(r'samples:\s*(\d+)', line)
            if samples_match:
                current_data['samples'] = int(samples_match.group(1))
                continue

            # Match latency values (avg, p50, p90, p99)
            latency_match = re.match(r'(avg|p50|p90|p99)\s+latency:\s*([\d.]+)\s*us', line)
            if latency_match:
                metric_type = latency_match.group(1)
                value = float(latency_match.group(2))
                current_data[metric_type] = value
                continue

        # Save last operation
        if current_op and len(current_data) >= 4:
            latency_metrics[current_op] = LatencyMetric(
                samples=current_data.get('samples', 0),
                avg_us=current_data.get('avg', 0.0),
                p50_us=current_data.get('p50', 0.0),
                p90_us=current_data.get('p90', 0.0),
                p99_us=current_data.get('p99', 0.0)
            )

        return latency_metrics