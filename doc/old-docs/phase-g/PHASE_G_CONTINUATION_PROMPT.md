# Phase G: Comprehensive Benchmark Suite - Continuation Prompt

**Date**: 2025-11-30  
**Branch**: `refactor/dwarfs-mkdwarfs-complete`  
**Phase**: G - Comprehensive Benchmark Suite

---

## Session Start

```bash
cd /Users/mulgogi/src/external/dwarfs
git branch --show-current
cat doc/PHASE_G_COMPREHENSIVE_BENCHMARK_PLAN.md
cat doc/PHASE_G_IMPLEMENTATION_STATUS.md
```

---

## Context

### Phases A-F Complete ✅
- FlatBuffers as default (102.91% size)
- File extensions implemented
- **Phase F Limitation**: Only tested tiny dataset, mkdwarfs only

### Phase G Required
Test **ALL 4 tools** with **MULTIPLE datasets**:
1. mkdwarfs (creation)
2. dwarfsck (verification)
3. dwarfsextract (extraction)
4. dwarfs (FUSE operations)

---

## Available Datasets

| Dataset | Files | Size | Type |
|---------|-------|------|------|
| Tiny | 11 | 213 B | Test |
| Perl 5.43.3 | 6,802 | ~95 MB | Source code |
| RasPi OS | 1 | ~2.7 GB | OS image |

---

## Implementation Tasks

### G1: Dataset Management (1-2h)
Download Perl dataset, verify checksums, document characteristics.

### G2: mkdwarfs Benchmarks (2-3h)
Create `comprehensive_mkdwarfs_benchmark.py`:
- Support multiple datasets
- Multiple compression levels (1,5,9)
- Memory tracking
- Save images for later tests

### G3: dwarfsck Benchmarks (1-2h)
Create `benchmark_dwarfsck.py`:
- Quick check (--check-integrity)
- Full validation
- JSON export

### G4: dwarfsextract Benchmarks (2-3h)
Create `benchmark_dwarfsextract.py`:
- Full extraction
- Selective extraction
- Archive conversion (tar)

### G5: dwarfs FUSE Benchmarks (3-4h)
Create `benchmark_dwarfs_fuse.py`:
- Mount time
- Sequential/random reads
- Directory traversal
- Metadata operations

Requires FUSE support (macFUSE/FUSE-T/WinFsp).

### G6: Comprehensive Reports (2-3h)
Create `generate_comprehensive_report.py`:
- Aggregate all benchmark data
- Format comparison matrix
- Performance profiles
- Recommendations

---

## Quick Start Example

```bash
# G1: Download dataset
python3 benchmarks/download_datasets.py --download perl

# G2: Run comprehensive mkdwarfs tests
python3 benchmarks/comprehensive_mkdwarfs_benchmark.py \
  --flatbuffers-mkdwarfs ./build-fb/mkdwarfs \
  --thrift-mkdwarfs ./build-tb/mkdwarfs \
  --datasets tiny,perl \
  --levels 1,5,9 \
  --iterations 5 \
  --save-images benchmark-results/images \
  --output benchmark-results/mkdwarfs-benchmarks.json

# G3: Test dwarfsck
python3 benchmarks/benchmark_dwarfsck.py \
  --dwarfsck ./build-fb/dwarfsck \
  --images benchmark-results/images/*.dff \
  --operations quick,full,json \
  --output benchmark-results/dwarfsck-benchmarks.json

# Continue with G4, G5, G6...
```

---

## Expected Outputs

```
benchmark-files/
├── perl-5.43.3/        # G1
└── DATASETS.md         # G1

benchmark-results/
├── mkdwarfs-benchmarks.json      # G2
├── dwarfsck-benchmarks.json      # G3
├── dwarfsextract-benchmarks.json # G4
├── dwarfs-fuse-benchmarks.json   # G5
└── images/                        # G2 creates
    ├── tiny-fb-l1.dff
    ├── tiny-tb-l1.dft
    ├── perl-fb-l5.dff
    └── ...

doc/
└── COMPREHENSIVE_BENCHMARK_REPORT.md # G6
```

---

## Timeline

| Day | Tasks | Hours |
|-----|-------|-------|
| 1 | G1+G2+G3 | 4-6h |
| 2 | G4+G5 | 5-7h |
| 3 | G6+Docs | 2-3h |

**Total**: 11-16 hours

---

## Validation

After Phase G:
- [ ] All datasets available
- [ ] mkdwarfs tested on all datasets (both formats)
- [ ] dwarfsck tested on all images
- [ ] dwarfsextract tested (full/selective/tar)
- [ ] dwarfs FUSE tested (if available)
- [ ] Comprehensive report generated
- [ ] Documentation updated

---

## Common Issues

**FUSE not available**: Skip G5, note in docs  
**Dataset too large**: Use only tiny dataset  
**Benchmarks too slow**: Reduce iterations to 1

---

**Start with G1** (Dataset Management) and work sequentially through G6.

See full details in:
- [`doc/PHASE_G_COMPREHENSIVE_BENCHMARK_PLAN.md`](PHASE_G_COMPREHENSIVE_BENCHMARK_PLAN.md)
- [`doc/PHASE_G_IMPLEMENTATION_STATUS.md`](PHASE_G_IMPLEMENTATION_STATUS.md)