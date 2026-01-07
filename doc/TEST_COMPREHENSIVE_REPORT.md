# DwarFS Comprehensive Benchmark Report

**Date**: 2025-11-30T12:20:34.296525Z  
**Platform**: Darwin arm64  
**Datasets**: tiny  
**Compression Levels**: 5  
**Iterations**: 1

---

## Executive Summary

### Key Findings

- **Fastest Creation**: thrift l5 (0.05s)
- **Smallest Image**: thrift l5 (1,239.0 bytes)
- **Fastest Verification**: flatbuffers-l5 (0.03s)

### Format Coverage
- **FlatBuffers**: 1 dataset(s) tested
- **Thrift**: 1 dataset(s) tested

## mkdwarfs Analysis (Creation)

### Dataset: tiny

| Format | Level | Time (s) | Image Size | Ratio | Throughput (MB/s) | Memory (MB) |
|--------|-------|----------|------------|-------|-------------------|-------------|
| flatbuffers | l5 | 0.12 | 2,028 | 0.1x | 0.0 | 24.6 |
| thrift | l5 | 0.05 | 1,239 | 0.2x | 0.0 | 25.5 |

## dwarfsck Analysis (Verification)

### Dataset: tiny

| Image | Quick Check (s) | Full Validation (s) | JSON Export (s) | JSON Size |
|-------|-----------------|---------------------|-----------------|-----------|
| flatbuffers-l5 | 0.03 | 0.02 | 0.02 | 0 |
| thrift-l5 | 0.03 | 0.02 | 0.02 | 0 |

## dwarfsextract Analysis (Extraction)

### Dataset: tiny

| Image | Extract All (s) | Throughput (MB/s) | To Tar (s) | Tar Throughput (MB/s) |
|-------|-----------------|-------------------|------------|------------------------|
| flatbuffers-l5 | 1.00 | 0.0 | 0.17 | 0.0 |
| thrift-l5 | 0.32 | 0.0 | 0.02 | 0.6 |

## dwarfs FUSE Analysis (Mount & Access)

## Format Comparison Matrix

Comparing FlatBuffers vs Thrift across all operations:

| Dataset | Level | Size Ratio (FB/TB) | Time Ratio (FB/TB) |
|---------|-------|--------------------|--------------------|
| tiny | 5 | 163.68% | 217.71% |
| **Average** | - | **163.68%** | **217.71%** |

**Interpretation**:
- FlatBuffers images are **163.7%** the size of Thrift (overhead: 63.7%)
- FlatBuffers creation is **217.7%** the time of Thrift

## Performance Profiles

### Tiny Dataset

**Strengths**:
- Image creation tested with 2 format(s)
- Verification tested on 2 image(s)
- Extraction tested on 2 image(s)

**Recommendations**:
- Best for: Quick testing

## Recommendations

### When to Use FlatBuffers
- ✅ **Default choice** for new projects
- ✅ Cross-platform portability requirements
- ✅ Simpler build dependencies
- ✅ Size overhead <110% of Thrift is acceptable

### When to Keep Thrift Support
- ⚠️ Reading legacy images created with Thrift
- ⚠️ Absolute maximum compression required
- ⚠️ Have Folly/fbthrift build infrastructure

### Performance Tuning Tips
- **Level 1**: Fastest creation, moderate compression
- **Level 5**: Balanced speed and compression (recommended)
- **Level 9**: Maximum compression, slower creation
- **FUSE cache**: Increase for better random read performance
- **Memory limit**: Set for large datasets to control resource usage