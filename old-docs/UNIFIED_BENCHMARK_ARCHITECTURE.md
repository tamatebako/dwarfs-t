# Unified DwarFS Benchmark Architecture

## Design Principles

### Core Principles (SOLID + Domain-Driven)

1. **Single Responsibility**: Each class has one clear purpose
2. **Open/Closed**: Ext ensible without modifying existing code
3. **MECE**: Mutually Exclusive, Collectively Exhaustive components
4. **Separation of Concerns**: Clear boundaries between modules
5. **Configuration over Code**: External YAML drives behavior
6. **Model-Based**: Domain models, not procedural functions
7. **Registry Pattern**: Plugin architecture for extensibility

### Problem with Current Dual Systems

**Current State**:
```
.benchmark/benchmark.py    benchmarks/run_metadata_format_benchmark.py
        ↓                              ↓
Decorator-based          Class-based but monolithic
Hardcoded benchmarks     Hardcoded format list
Version comparison       Format comparison
Separate metrics         Separate metrics
                ↓
        DUPLICATION & HARDCODING
```

**Issues**:
- Two different architectures for same domain (benchmarking)
- Hardcoded benchmark lists
- Hardcoded metrics
- Hardcoded datasets
- Duplicated execution logic
- No shared domain model

---

## Unified Architecture

### High-Level Domain Model

```
┌────────────────────────────────────────────────────────────────┐
│                      Benchmark System                           │
└────────────────────────────────────────────────────────────────┘
                              │
                         ┌────▼────┐
                         │ Profile │  ◄─── NEW: Predefined benchmark suites
                         │  Model  │       (quick check, release validation, etc.)
                         └────┬────┘
                              │ orchestrates
        ┌─────────────────────┼──────────────────┐
        │                     │                  │
   ┌────▼────┐          ┌────▼────┐       ┌────▼────┐
   │ Dataset │          │Benchmark│       │ Metric  │
   │  Model  │          │  Model  │       │  Model  │
   └─────────┘          └─────────┘       └─────────┘
        │                     │                  │
        │                     │                  │
   ┌────▼────┐          ┌────▼────┐       ┌────▼────┐
   │ Dataset │          │Benchmark│       │ Metric  │
   │Registry │          │Executor │       │Collector│
   └─────────┘          └─────────┘       └─────────┘
                              │
                         ┌────▼────┐
                         │ Result  │
                         │  Model  │
                         └─────────┘
                              │
                         ┌────▼────┐
                         │ Report  │
                         │Generator│
                         └─────────┘
```

### Core Domain Models

#### 1. BenchmarkProfile Model (NEW - Orchestration Layer)

```yaml
# benchmarks/config/profiles.yaml
profiles:
  quick_check:
    name: "Quick Performance Check"
    description: "Fast validation for development (< 5 minutes)"
    benchmarks:
      - metadata_format_comparison
    datasets:
      - perl  # Small dataset only
    metrics:
      - compression_time
      - compression_size
    runs: 1
    warmup_runs: 0
    report_template: "quick_check"

  release_validation:
    name: "Release Validation Suite"
    description: "Comprehensive validation before release (30-45 minutes)"
    benchmarks:
      - metadata_format_comparison
      - version_regression
    datasets:
      - perl
      - raspios
    metrics:
      - compression_time
      - compression_size
      - compression_memory
      - extraction_time
      - extraction_throughput
      - fuse_mount_time
      - fuse_read_latency
    runs: 3
    warmup_runs: 1
    report_template: "comprehensive"

  deep_dive:
    name: "Performance Deep Dive"
    description: "Exhaustive analysis with all metrics (2-3 hours)"
    benchmarks:
      - metadata_format_comparison
      - compression_algorithm_sweep
      - block_size_optimization
    datasets:
      - perl
      - raspios
      - linux_kernel  # If available
    metrics: "all"  # All defined metrics
    runs: 5
    warmup_runs: 2
    report_template: "deep_dive"

  ci_smoke:
    name: "CI Smoke Test"
    description: "Lightweight CI validation (< 2 minutes)"
    benchmarks:
      - metadata_format_comparison
    datasets:
      - perl
    metrics:
      - compression_time
    runs: 1
    warmup_runs: 0
    report_template: "ci_summary"
```

**Python Model**:
```python
class BenchmarkProfile:
    """Orchestrates a predefined set of benchmarks for specific use case"""

    def __init__(self, id: str, config: Dict[str, Any]):
        self.id = id
        self.name = config['name']
        self.description = config['description']
        self.benchmark_ids = config['benchmarks']
        self.dataset_ids = config['datasets']
        self.metric_ids = config['metrics']
        self.runs = config['runs']
        self.warmup_runs = config['warmup_runs']
        self.report_template = config['report_template']

    def get_benchmarks(self) -> list[Benchmark]:
        """Get all benchmarks in this profile"""
        return [BenchmarkRegistry.get(b) for b in self.benchmark_ids]

    def get_datasets(self) -> list[Dataset]:
        """Get all datasets for this profile"""
        return [DatasetRegistry.get(d) for d in self.dataset_ids]

    def get_metrics(self) -> list[Metric]:
        """Get all metrics to measure"""
        if self.metric_ids == "all":
            return MetricRegistry.all()
        return [MetricRegistry.get(m) for m in self.metric_ids]

    def execute(self, context: ProfileContext) -> ProfileResult:
        """Execute complete profile"""
        results = []

        for benchmark in self.get_benchmarks():
            # Configure benchmark with profile settings
            benchmark_context = BenchmarkContext(
                datasets=self.get_datasets(),
                metrics=self.get_metrics(),
                runs=self.runs,
                warmup_runs=self.warmup_runs,
                binaries=context.binaries
            )

            result = benchmark.execute(benchmark_context)
            results.append(result)

        return ProfileResult(
            profile_id=self.id,
            benchmark_results=results,
            timestamp=datetime.now()
        )

    def generate_report(self, results: ProfileResult) -> str:
        """Generate report using configured template"""
        template = ReportTemplateRegistry.get(self.report_template)
        generator = ReportGeneratorFactory.create(template.format)
        return generator.generate(results, template)


class ProfileRegistry:
    """Registry of benchmark profiles"""
    _profiles: Dict[str, BenchmarkProfile] = {}

    @classmethod
    def load(cls, config_path: Path):
        with open(config_path) as f:
            config = yaml.safe_load(f)

        for profile_id, profile_config in config['profiles'].items():
            profile_config['id'] = profile_id
            cls._profiles[profile_id] = BenchmarkProfile(profile_id, profile_config)

    @classmethod
    def get(cls, profile_id: str) -> BenchmarkProfile:
        if profile_id not in cls._profiles:
            raise ValueError(f"Unknown profile: {profile_id}")
        return cls._profiles[profile_id]

    @classmethod
    def all(cls) -> list[BenchmarkProfile]:
        return list(cls._profiles.values())
```

#### 2. Dataset Model

```yaml
# benchmarks/config/datasets.yaml
datasets:
  perl:
    name: "Perl 5.43.3 Source"
    type: "source_tree"
    source:
      url: "https://www.cpan.org/src/5.0/perl-5.43.3.tar.gz"
      sha256: "318651ee..."
    path: "benchmark-files/perl-5.43.3"
    test_files:
      small: "doio.c"
      medium: "pp.c"
      large: "perl"
    metadata:
      file_count: 6802
      total_size_mb: 95.19
      characteristics:
        - many_small_files
        - text_heavy
        - high_compressibility

  raspios:
    name: "Raspberry Pi OS Lite ARM64"
    type: "disk_image"
    source:
      url: "https://downloads.raspberrypi.com/..."
      sha256: "79146135..."
    path: "benchmark-files/raspios_dataset"
    test_files:
      large: "raspios_sample.img"
    metadata:
      file_count: 1
      total_size_mb: 2730
      characteristics:
        - single_large_file
        - binary_data
        - low_compressibility
```

**Python Model**:
```python
class Dataset:
    """Domain model for benchmark dataset"""
    def __init__(self, config: dict):
        self.id = config['id']
        self.name = config['name']
        self.type = config['type']
        self.source = DatasetSource(config['source'])
        self.path = Path(config['path'])
        self.test_files = config['test_files']
        self.metadata = DatasetMetadata(config['metadata'])

    def exists(self) -> bool:
        return self.path.exists()

    def download(self) -> bool:
        return self.source.download(self.path)
```

#### 2. Benchmark Model

```yaml
# benchmarks/config/benchmarks.yaml
benchmarks:
  metadata_format_comparison:
    name: "Metadata Format Comparison"
    type: "format_comparison"
    dimensions:
      - format  # thrift, cereal, bitsery
      - dataset # perl, raspios
    metrics:
      - compression_time
      - compression_size
      - compression_memory
      - extraction_time
      - fuse_mount_time
      - fuse_read_latency
    runs: 3
    warmup_runs: 1

  version_regression:
    name: "Version Performance Regression"
    type: "version_comparison"
    dimensions:
      - version
      - configuration
    metrics:
      - compression_time
      - binary_size
    runs: 5
    warmup_runs: 2
```

**Python Model**:
```python
class Benchmark:
    """Domain model for benchmark definition"""
    def __init__(self, config: dict):
        self.id = config['id']
        self.name = config['name']
        self.type = config['type']
        self.dimensions = config['dimensions']
        self.metrics = [MetricRegistry.get(m) for m in config['metrics']]
        self.runs = config['runs']
        self.warmup_runs = config['warmup_runs']

    def execute(self, context: BenchmarkContext) -> BenchmarkResult:
        executor = BenchmarkExecutorFactory.create(self.type)
        return executor.execute(self, context)
```

#### 3. Metric Model

```yaml
# benchmarks/config/metrics.yaml
metrics:
  compression_time:
    name: "Compression Time"
    unit: "seconds"
    aggregation: "mean"
    comparison: "lower_is_better"
    collector: "TimeMetricCollector"

  compression_size:
    name: "Archive Size"
    unit: "bytes"
    aggregation: "exact"
    comparison: "lower_is_better"
    collector: "SizeMetricCollector"

  fuse_read_latency:
    name: "FUSE Read Latency"
    unit: "microseconds"
    aggregation: "percentiles"
    percentiles: [50, 90, 99]
    comparison: "lower_is_better"
    collector: "PerfmonLatencyCollector"
```

**Python Model**:
```python
class Metric:
    """Domain model for measurable metric"""
    def __init__(self, config: dict):
        self.id = config['id']
        self.name = config['name']
        self.unit = config['unit']
        self.aggregation = config['aggregation']
        self.comparison = config['comparison']
        self.collector_class = config['collector']

    def collect(self, context: MetricContext) -> MetricValue:
        collector = MetricCollectorRegistry.get(self.collector_class)
        return collector.collect(context)
```

### Proper OO Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                        Execution Layer                           │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  BenchmarkExecutor (Abstract)                                    │
│      ├─ FormatComparisonExecutor                                │
│      ├─ VersionComparisonExecutor                               │
│      └─ ConfigurationComparisonExecutor                         │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
                           │
┌─────────────────────────────────────────────────────────────────┐
│                      Measurement Layer                           │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  MetricCollector (Abstract)                                      │
│      ├─ TimeMetricCollector                                     │
│      ├─ MemoryMetricCollector                                   │
│      ├─ SizeMetricCollector                                     │
│      ├─ PerfmonLatencyCollector                                 │
│      └─ ThroughputMetricCollector                               │
│                                                                  │
│  Measurement Tools:                                              │
│      ├─ MemoryTracker (cross-platform /usr/bin/time)            │
│      ├─ PerfmonParser (FUSE metrics)                            │
│      └─ FUSEManager (mount/unmount)                             │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
                           │
┌─────────────────────────────────────────────────────────────────┐
│                        Registry Layer                            │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  DatasetRegistry      - Load from datasets.yaml                 │
│  BenchmarkRegistry    - Load from benchmarks.yaml               │
│  MetricRegistry       - Load from metrics.yaml                  │
│  ExecutorRegistry     - Map benchmark types to executors        │
│  CollectorRegistry    - Map metrics to collectors               │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
                           │
┌─────────────────────────────────────────────────────────────────┐
│                        Reporting Layer                           │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  ReportGenerator (Abstract)                                      │
│      ├─ MarkdownReportGenerator                                │
│      ├─ JSONReportGenerator                                     │
│      └─ SVGChartGenerator                                       │
│                                                                  │
│  Report Templates (External Config):                            │
│      ├─ format_comparison_template.yaml                         │
│      ├─ version_regression_template.yaml                        │
│      └─ custom_report_template.yaml                             │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

### Configuration-Driven System

All aspects controlled by external configuration:

**1. What to Benchmark** (`benchmarks.yaml`)
**2. What Datasets** (`datasets.yaml`)
**3. What Metrics** (`metrics.yaml`)
**4. How to Report** (`reports.yaml`)

```yaml
# benchmarks/config/reports.yaml
reports:
  format_comparison:
    type: "comparison_matrix"
    baseline_dimension: "format"
    baseline_value: "thrift"
    sections:
      - datasets
      - compression_metrics
      - extraction_metrics
      - fuse_metrics
    output_formats:
      - markdown
      - json
      - svg_charts

  version_regression:
    type: "time_series"
    x_axis: "version"
    metrics:
      - compression_time
      - binary_size
    output_formats:
      - markdown
      - json
```

### Extensibility Example

**Adding New Metric** (Zero Code Changes):

```yaml
# Just add to metrics.yaml
metrics:
  cache_hit_ratio:
    name: "Cache Hit Ratio"
    unit: "percentage"
    aggregation: "mean"
    comparison: "higher_is_better"
    collector: "CacheStatsCollector"
```

```python
# Implement collector (follows interface)
class CacheStatsCollector(MetricCollector):
    def collect(self, context: MetricContext) -> MetricValue:
        # Implementation
        pass

# Register (auto-discovered via plugin pattern)
MetricCollectorRegistry.register('CacheStatsCollector', CacheStatsCollector)
```

**Adding New Dataset** (Just YAML):

```yaml
datasets:
  linux_kernel:
    name: "Linux Kernel 6.1"
    type: "source_tree"
    source:
      url: "https://cdn.kernel.org/pub/linux/kernel/v6.x/linux-6.1.tar.xz"
      sha256: "..."
    path: "benchmark-files/linux-6.1"
    test_files:
      small: "Makefile"
      large: "vmlinux"
```

**Adding New Benchmark** (Just YAML):

```yaml
benchmarks:
  compression_algorithm_comparison:
    name: "Compression Algorithm Comparison"
    type: "parameter_sweep"
    dimensions:
      - algorithm  # zstd, lzma, brotli
      - level     # 1-9
      - dataset
    metrics:
      - compression_time
      - compression_ratio
    runs: 3
    warmup_runs: 1
```

### Single Entry Point

```bash
# Unified command for ALL benchmarks
python3 benchmarks/run.py \
  --benchmark metadata_format_comparison \
  --config benchmarks/config/benchmarks.yaml \
  --output results.json

# Or run all configured benchmarks
python3 benchmarks/run.py --all

# Generate any report type
python3 benchmarks/report.py \
  --template format_comparison \
  --results results.json \
  --output REPORT.md
```

### Migration Strategy

#### Phase 1: Core Framework
1. Create domain models: `Dataset`, `Benchmark`, `Metric`, `Result`
2. Implement registries: Load from YAML configs
3. Create abstract base classes: `BenchmarkExecutor`, `MetricCollector`, `ReportGenerator`

#### Phase 2: Migrate Existing Benchmarks
1. Convert `.benchmark/benchmark.py` functions to YAML config + executors
2. Convert `benchmarks/run_metadata_format_benchmark.py` to `FormatComparisonExecutor`
3. All existing functionality preserved, now extensible

#### Phase 3: Enhanced Reporting
1. Template-based report generation
2. Multiple output formats from same data
3. Custom report types via configuration

---

## Detailed Class Structure

### Core Domain Models

```python
# benchmarks/lib/models/dataset.py
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, Any

@dataclass
class DatasetSource:
    url: str
    sha256: str

@dataclass
class DatasetMetadata:
    file_count: int
    total_size_mb: float
    characteristics: list[str]

class Dataset:
    """Immutable value object representing a benchmark dataset"""
    def __init__(self, id: str, config: Dict[str, Any]):
        self.id = id
        self.name = config['name']
        self.type = config['type']
        self.source = DatasetSource(**config['source'])
        self.path = Path(config['path'])
        self.test_files = config['test_files']
        self.metadata = DatasetMetadata(**config['metadata'])

    def exists(self) -> bool:
        return self.path.exists()

    def get_test_file(self, size_category: str) -> str:
        return self.test_files.get(size_category)
```

```python
# benchmarks/lib/models/metric.py
from abc import ABC, abstractmethod
from enum import Enum

class AggregationType(Enum):
    MEAN = "mean"
    MEDIAN = "median"
    PERCENTILES = "percentiles"
    EXACT = "exact"

class ComparisonType(Enum):
    LOWER_IS_BETTER = "lower_is_better"
    HIGHER_IS_BETTER = "higher_is_better"

class Metric:
    """Configuration for a measurable metric"""
    def __init__(self, id: str, config: Dict[str, Any]):
        self.id = id
        self.name = config['name']
        self.unit = config['unit']
        self.aggregation = AggregationType(config['aggregation'])
        self.comparison = ComparisonType(config['comparison'])
        self.collector_class = config['collector']

    def create_collector(self) -> 'MetricCollector':
        return MetricCollectorRegistry.get(self.collector_class)()
```

```python
# benchmarks/lib/models/benchmark.py
class Benchmark:
    """Configuration for a benchmark suite"""
    def __init__(self, id: str, config: Dict[str, Any]):
        self.id = id
        self.name = config['name']
        self.type = config['type']
        self.dimensions = config['dimensions']
        self.metric_ids = config['metrics']
        self.runs = config['runs']
        self.warmup_runs = config['warmup_runs']

    def get_metrics(self) -> list[Metric]:
        return [MetricRegistry.get(m) for m in self.metric_ids]

    def create_executor(self) -> 'BenchmarkExecutor':
        return BenchmarkExecutorRegistry.get(self.type)()
```

### Registries (Single Source of Truth)

```python
# benchmarks/lib/registries.py
import yaml
from pathlib import Path
from typing import Dict, Type

class DatasetRegistry:
    """Registry of all available datasets (loaded from YAML)"""
    _datasets: Dict[str, Dataset] = {}

    @classmethod
    def load(cls, config_path: Path):
        with open(config_path) as f:
            config = yaml.safe_load(f)

        for dataset_id, dataset_config in config['datasets'].items():
            dataset_config['id'] = dataset_id
            cls._datasets[dataset_id] = Dataset(dataset_id, dataset_config)

    @classmethod
    def get(cls, dataset_id: str) -> Dataset:
        if dataset_id not in cls._datasets:
            raise ValueError(f"Unknown dataset: {dataset_id}")
        return cls._datasets[dataset_id]

    @classmethod
    def all(cls) -> list[Dataset]:
        return list(cls._datasets.values())


class MetricRegistry:
    """Registry of all measurable metrics"""
    _metrics: Dict[str, Metric] = {}

    @classmethod
    def load(cls, config_path: Path):
        with open(config_path) as f:
            config = yaml.safe_load(f)

        for metric_id, metric_config in config['metrics'].items():
            metric_config['id'] = metric_id
            cls._metrics[metric_id] = Metric(metric_id, metric_config)

    @classmethod
    def get(cls, metric_id: str) -> Metric:
        if metric_id not in cls._metrics:
            raise ValueError(f"Unknown metric: {metric_id}")
        return cls._metrics[metric_id]


class BenchmarkRegistry:
    """Registry of all benchmark suites"""
    _benchmarks: Dict[str, Benchmark] = {}

    @classmethod
    def load(cls, config_path: Path):
        with open(config_path) as f:
            config = yaml.safe_load(f)

        for bench_id, bench_config in config['benchmarks'].items():
            bench_config['id'] = bench_id
            cls._benchmarks[bench_id] = Benchmark(bench_id, bench_config)

    @classmethod
    def get(cls, benchmark_id: str) -> Benchmark:
        if benchmark_id not in cls._benchmarks:
            raise ValueError(f"Unknown benchmark: {benchmark_id}")
        return cls._benchmarks[benchmark_id]

    @classmethod
    def all(cls) -> list[Benchmark]:
        return list(cls._benchmarks.values())
```

### Executors (Strategy Pattern)

```python
# benchmarks/lib/executors/base.py
from abc import ABC, abstractmethod

class BenchmarkExecutor(ABC):
    """Abstract base for benchmark execution strategies"""

    @abstractmethod
    def execute(self, benchmark: Benchmark, context: BenchmarkContext) -> BenchmarkResult:
        """Execute benchmark and return results"""
        pass


# benchmarks/lib/executors/format_comparison.py
class FormatComparisonExecutor(BenchmarkExecutor):
    """Execute format comparison benchmarks"""

    def execute(self, benchmark: Benchmark, context: BenchmarkContext) -> BenchmarkResult:
        results = []

        # Get all combinations of dimensions
        for dataset_id in context.get_dimension_values('dataset'):
            dataset = DatasetRegistry.get(dataset_id)

            for format_name in context.get_dimension_values('format'):
                # Execute all configured metrics
                metric_values = {}
                for metric in benchmark.get_metrics():
                    collector = metric.create_collector()
                    metric_values[metric.id] = collector.collect(
                        MetricContext(
                            dataset=dataset,
                            format=format_name,
                            runs=benchmark.runs,
                            warmup_runs=benchmark.warmup_runs
                        )
                    )

                results.append(BenchmarkSample(
                    dataset=dataset_id,
                    format=format_name,
                    metrics=metric_values
                ))

        return BenchmarkResult(
            benchmark_id=benchmark.id,
            samples=results
        )


# benchmarks/lib/executors/registry.py
class BenchmarkExecutorRegistry:
    """Maps benchmark types to executor implementations"""
    _executors: Dict[str, Type[BenchmarkExecutor]] = {}

    @classmethod
    def register(cls, benchmark_type: str, executor_class: Type[BenchmarkExecutor]):
        cls._executors[benchmark_type] = executor_class

    @classmethod
    def get(cls, benchmark_type: str) -> Type[BenchmarkExecutor]:
        if benchmark_type not in cls._executors:
            raise ValueError(f"Unknown benchmark type: {benchmark_type}")
        return cls._executors[benchmark_type]

# Auto-register executors
BenchmarkExecutorRegistry.register('format_comparison', FormatComparisonExecutor)
BenchmarkExecutorRegistry.register('version_comparison', VersionComparisonExecutor)
```

### Collectors (Template Method Pattern)

```python
# benchmarks/lib/collectors/base.py
class MetricCollector(ABC):
    """Abstract base for metric collection"""

    @abstractmethod
    def collect(self, context: MetricContext) -> MetricValue:
        """Collect metric value from execution context"""
        pass


# benchmarks/lib/collectors/time_collector.py
class TimeMetricCollector(MetricCollector):
    """Collect time-based metrics using MemoryTracker"""

    def __init__(self):
        self.tracker = MemoryTracker()

    def collect(self, context: MetricContext) -> MetricValue:
        # Build command based on context
        cmd = context.build_command()

        # Execute with multiple runs
        times = []
        for run in range(context.runs):
            result = self.tracker.measure_command(cmd)
            times.append(result['wall_time'])

        # Discard warmup runs
        times = times[context.warmup_runs:]

        return MetricValue(
            value=statistics.mean(times),
            std_dev=statistics.stdev(times) if len(times) > 1 else 0,
            unit='seconds'
        )


# benchmarks/lib/collectors/perfmon_collector.py
class PerfmonLatencyCollector(MetricCollector):
    """Collect FUSE latency metrics via perfmon"""

    def __init__(self):
        self.fuse_manager = FUSEManager()
        self.perfmon_parser = PerfmonParser()

    def collect(self, context: MetricContext) -> MetricValue:
        # Mount, run operations, collect perfmon
        with self.fuse_manager.mount_context(context.image_path) as mount_point:
            # Run test workload
            context.execute_fuse_workload(mount_point)

            # Collect perfmon data
            perfmon_data = self.fuse_manager.get_perfmon_data(mount_point)
            latency_metrics = self.perfmon_parser.parse(perfmon_data)

        return MetricValue(
            value=latency_metrics['read'].avg_us,
            percentiles={
                'p50': latency_metrics['read'].p50_us,
                'p90': latency_metrics['read'].p90_us,
                'p99': latency_metrics['read'].p99_us
            },
            unit='microseconds'
        )
```

### Report Generation (Strategy + Template Pattern)

```python
# benchmarks/lib/reporting/base.py
class ReportGenerator(ABC):
    """Abstract base for report generation"""

    @abstractmethod
    def generate(self, results: BenchmarkResult, template: ReportTemplate) -> str:
        """Generate report from results using template"""
        pass


# benchmarks/lib/reporting/markdown_generator.py
class MarkdownReportGenerator(ReportGenerator):
    """Generate Markdown reports from templates"""

    def generate(self, results: BenchmarkResult, template: ReportTemplate) -> str:
        # Load template configuration
        sections = []
        for section_config in template.sections:
            section_generator = self._get_section_generator(section_config['type'])
            sections.append(section_generator.generate(results, section_config))

        return '\n\n'.join(sections)

    def _get_section_generator(self, section_type: str):
        # Factory for section generators
        return SectionGeneratorRegistry.get(section_type)()
```

---

## Proposed File Structure

```
benchmarks/
├── run.py                      # Unified entry point
├── report.py                   # Report generation entry point
├── config/                     # EXTERNAL CONFIGURATION
│   ├── datasets.yaml           # All datasets
│   ├── benchmarks.yaml         # All benchmark definitions
│   ├── metrics.yaml            # All metric definitions
│   └── reports.yaml            # Report templates
├── lib/                        # Shared library (EXPANDED)
│   ├── __init__.py
│   ├── models/                 # Domain models
│   │   ├── dataset.py
│   │   ├── benchmark.py
│   │   ├── metric.py
│   │   └── result.py
│   ├── registries.py           # Configuration loaders & registries
│   ├── executors/              # Benchmark execution strategies
│   │   ├── base.py
│   │   ├── format_comparison.py
│   │   ├── version_comparison.py
│   │   └── parameter_sweep.py
│   ├── collectors/             # Metric collection implementations
│   │   ├── base.py
│   │   ├── time_collector.py
│   │   ├── memory_collector.py
│   │   ├── perfmon_collector.py
│   │   └── size_collector.py
│   ├── tools/                  # Low-level measurement tools
│   │   ├── memory_tracker.py   (existing)
│   │   ├── perfmon_parser.py   (existing)
│   │   ├── fuse_manager.py     (existing)
│   │   └── progress.py         (existing)
│   ├── reporting/              # Report generation
│   │   ├── base.py
│   │   ├── markdown_generator.py
│   │   ├── json_generator.py
│   │   └── svg_generator.py
│   └── downloaders/            # Dataset management
│       └── dataset_downloader.py (existing)
└── README.md

.benchmark/                     # REMOVED (migrated to unified system)
```

---

## Benefits of Unified Architecture

### 1. Extensibility
- Add dataset: Edit YAML only
- Add metric: Implement collector, add YAML
- Add benchmark type: Implement executor, add YAML
- Add report format: Implement generator, add template

### 2. Maintainability
- Single source of truth for each concept
- Clear separation of concerns
- No duplicated logic
- Easy to test (models are pure)

### 3. Configurability
- Everything driven by external config
- No code changes for new benchmarks
- Easy to customize reports
- Simple A/B testing

### 4. Reusability
- Collectors work with any benchmark
- Executors reuse collectors
- Report generators work with any results
- Tools (memory_tracker, etc.) used by all collectors

---

## Implementation Roadmap

### Week 1: Core Framework
- [ ] Create domain models (`Dataset`, `Benchmark`, `Metric`, `Result`)
- [ ] Implement registries with YAML loading
- [ ] Create abstract base classes for executors and collectors
- [ ] Write comprehensive tests for models

### Week 2: Migrate Metadata Benchmarks
- [ ] Implement `FormatComparisonExecutor`
- [ ] Port metric collection to collector classes
- [ ] Create YAML configs for existing metadata benchmarks
- [ ] Verify functional parity with `run_metadata_format_benchmark.py`

### Week 3: Migrate Version Benchmarks
- [ ] Implement `VersionComparisonExecutor`
- [ ] Port `.benchmark/benchmark.py` functions to YAML + executors
- [ ] Verify functional parity
- [ ] Remove old `.benchmark/` system

### Week 4: Enhanced Reporting
- [ ] Implement template-based report generation
- [ ] Create report templates for common use cases
- [ ] Add SVG chart generation
- [ ] Documentation and examples

### Week 5: Polish & Documentation
- [ ] Comprehensive README with examples
- [ ] Migration guide from old systems
- [ ] Tutorial for adding new benchmarks
- [ ] CI/CD integration examples

---

## Backward Compatibility

During migration, maintain:

```python
# Legacy wrapper (temporary)
def run_metadata_format_benchmark(**kwargs):
    """Backward-compatible wrapper"""
    import warnings
    warnings.warn("Use benchmarks/run.py --benchmark metadata_format_comparison")

    # Map old arguments to new system
    benchmark = BenchmarkRegistry.get('metadata_format_comparison')
    context = BenchmarkContext.from_legacy_args(**kwargs)
    return benchmark.execute(context)
```

---

## Success Criteria

- [ ] Single `benchmarks/run.py` entry point for all benchmarks
- [ ] Zero hardcoded datasets, metrics, or benchmarks
- [ ] All configuration in YAML files
- [ ] <500 lines per module (proper separation)
- [ ] Full test coverage
- [ ] Migration documentation
- [ ] No loss of existing functionality

---

## Conclusion

The unified architecture eliminates duplication, enables extensibility through configuration, and follows proper OO principles. The migration is straightforward and can be done incrementally without breaking existing functionality.

**Recommendation: Proceed with unified architecture implementation.**

---

## Usage Examples with Profiles

### Profile-Based Execution (Recommended)

```bash
# Quick check during development
benchmarks/run.py --profile quick_check
# Output: benchmarks/results/quick_check_20251105.json
# Auto-generates: QUICK_CHECK_REPORT.md

# Release validation before tagging
benchmarks/run.py --profile release_validation
# Runs: Both datasets, all formats, standard metrics
# Output: Comprehensive report with recommendations

# Deep performance analysis
benchmarks/run.py --profile deep_dive
# Runs: All datasets, all benchmarks, all metrics
# Output: Exhaustive analysis with charts

# CI smoke test
benchmarks/run.py --profile ci_smoke
# Fast validation for pull requests
```

### List Available Options

```bash
# List all profiles
benchmarks/run.py --list-profiles
# Output:
#   quick_check      - Fast validation (< 5 min)
#   release_validation - Comprehensive (30-45 min)
#   deep_dive        - Exhaustive (2-3 hours)
#   ci_smoke         - Lightweight CI (< 2 min)

# List all benchmarks
benchmarks/run.py --list-benchmarks

# List all datasets
benchmarks/run.py --list-datasets

# List all metrics
benchmarks/run.py --list-metrics
```

### Custom Profile Execution

```bash
# Override profile settings
benchmarks/run.py --profile quick_check --runs 3

# Run profile with different datasets
benchmarks/run.py --profile quick_check --datasets perl,raspios

# Run profile with specific metrics only
benchmarks/run.py --profile quick_check --metrics compression_time,compression_size
```

### Manual Benchmark Execution

```bash
# Run specific benchmark manually
benchmarks/run.py \
  --benchmark metadata_format_comparison \
  --datasets perl \
  --metrics compression_time,extraction_time \
  --runs 3 \
  --output custom_results.json

# Generate custom report
benchmarks/report.py \
  --results custom_results.json \
  --template format_comparison \
  --output CUSTOM_REPORT.md
```

## Profile Use Cases

### Development Workflow

```bash
# 1. Make code changes
vim src/metadata/...

# 2. Quick validation
benchmarks/run.py --profile quick_check
# ✓ Fast feedback (< 5 min)

# 3. If looks good, full validation
benchmarks/run.py --profile release_validation
# ✓ Comprehensive check before PR
```

### Release Process

```bash
# 1. Run release validation
benchmarks/run.py --profile release_validation

# 2. Review report
cat RELEASE_VALIDATION_REPORT.md

# 3. If acceptable, tag release
git tag v0.15.0

# 4. Archive results
cp benchmarks/results/release_validation_*.json \
   benchmarks/archive/v0.15.0_results.json
```

### Performance Investigation

```bash
# Noticed slowdown? Deep dive
benchmarks/run.py --profile deep_dive

# Compare with previous release
benchmarks/report.py \
  --results benchmarks/results/deep_dive_latest.json \
  --baseline benchmarks/archive/v0.14.0_results.json \
  --template regression_analysis \
  --output REGRESSION_REPORT.md
```

### CI Integration

```yaml
# .github/workflows/benchmark.yml
- name: Run CI Smoke Test
  run: |
    python3 benchmarks/run.py --profile ci_smoke

- name: Upload Results
  uses: actions/upload-artifact@v3
  with:
    name: benchmark-results
    path: benchmarks/results/ci_smoke_*.json
```

---

## Architecture Diagram: Profile Execution Flow

```
User
  │
  ├─ run.py --profile quick_check
  │
  ▼
ProfileRegistry.get('quick_check')
  ↓
BenchmarkProfile
  ├─ benchmarks: [metadata_format_comparison]
  ├─ datasets: [perl]
  ├─ metrics: [compression_time, compression_size]
  ├─ runs: 1
  ├─ warmup_runs: 0
  └─ report_template: "quick_check"
  │
  ├─ profile.execute(context)
  │   │
  │   ├─ For each benchmark:
  │   │   ├─ Get executor: FormatComparisonExecutor
  │   │   ├─ For each format × dataset:
  │   │   │   ├─ For each metric:
  │   │   │   │   ├─ Get collector: TimeMetricCollector
  │   │   │   │   ├─ collector.collect() → uses MemoryTracker
  │   │   │   │   └─ Store MetricValue
  │   │   │   └─ Create BenchmarkSample
  │   │   └─ Create BenchmarkResult
  │   └─ Create ProfileResult
  │
  └─ profile.generate_report(results)
      ├─ Get template: ReportTemplateRegistry.get('quick_check')
      ├─ Get generator: MarkdownReportGenerator
      └─ Generate QUICK_CHECK_REPORT.md
```

---

## Configuration Schema Summary

```
benchmarks/config/
├── profiles.yaml        # Predefined benchmark suites
│   └─ Defines: quick_check, release_validation, deep_dive, ci_smoke
├── benchmarks.yaml      # Individual benchmark definitions
│   └─ Defines: metadata_format_comparison, version_regression, etc.
├── datasets.yaml        # Available datasets
│   └─ Defines: perl, raspios, linux_kernel, etc.
├── metrics.yaml         # Measurable metrics
│   └─ Defines: compression_time, fuse_latency, etc.
└── reports.yaml         # Report templates
    └─ Defines: format_comparison, regression_analysis, etc.
```

**Hierarchy**:
```
Profile
  ├─ Contains multiple Benchmarks
  │   ├─ Each Benchmark uses Datasets
  │   └─ Each Benchmark measures Metrics
  └─ Generates Report using Template
```

**Everything is configuration-driven, nothing is hardcoded.**