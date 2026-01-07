# DwarFS Extract Fix & Benchmarking - Continuation Plan

**Date**: 2025-12-04  
**Status**: 🔴 **Phase 1 Blocked - Proceeding with Phase 2**  
**Objective**: Complete Thrift header → Enable 3-build benchmarking

---

## Current Situation

### Phase 1: dwarfsextract Bug - BLOCKED ⏸️

**Issue**: Pre-existing segmentation fault prevents extraction
- ✅ Identified root cause (directory creation)
- ✅ Applied partial fix
- ❌ Deeper segfault remains (requires ASAN/upstream help)

**Decision**: **DEFER Phase 1** - Use mount+copy workaround for now

### Path Forward

**Proceed with Phase 2** (independent work) → Complete benchmarking using workaround

---

## Revised Execution Plan

### Phase 2: Create Missing Thrift Header (1-2 hours) ⏭️ NEXT

**Objective**: Enable Thrift/dual-format builds

**File to Create**: `include/dwarfs/writer/internal/thrift_metadata_builder_impl.h`

**Steps**:

1. **Extract Class Declaration** (30 min)
   - Read [`src/writer/internal/thrift_metadata_builder.cpp`](../src/writer/internal/thrift_metadata_builder.cpp)
   - Find `thrift_metadata_builder` class (line ~234, anonymous namespace)
   - Extract class interface to header

2. **Create Header File** (30 min)
   - Use [`flatbuffers_metadata_builder_impl.h`](../include/dwarfs/writer/internal/flatbuffers_metadata_builder_impl.h) as template
   - Declare all public methods
   - Declare all private members
   - Add proper documentation

3. **Update Source Files** (15 min)
   - Modify [`thrift_metadata_builder.cpp`](../src/writer/internal/thrift_metadata_builder.cpp): Remove from anon namespace, add header include
   - Update [`metadata_builder.cpp:46`](../src/writer/internal/metadata_builder.cpp:46): Change include path
   - Update [`metadata_builder_factory.cpp:43`](../src/writer/internal/metadata_builder_factory.cpp:43): Change include path

4. **Test Builds** (15 min)
   ```bash
   # Thrift-only
   cmake -B build-tb -GNinja \
     -DDWARFS_WITH_FLATBUFFERS=OFF \
     -DDWARFS_WITH_THRIFT=ON \
     -DCMAKE_BUILD_TYPE=Release \
     -DWITH_TESTS=OFF
   ninja -C build-tb mkdwarfs dwarfsck
   
   # Dual-format
   cmake -B build-dual -GNinja \
     -DDWARFS_WITH_FLATBUFFERS=ON \
     -DDWARFS_WITH_THRIFT=ON \
     -DCMAKE_BUILD_TYPE=Release \
     -DWITH_TESTS=OFF
   ninja -C build-dual mkdwarfs dwarfsck
   ```

**Deliverables**:
- ✅ `include/dwarfs/writer/internal/thrift_metadata_builder_impl.h` (est. 250 lines)
- ✅ All 3 builds working (fb, tb, dual)

---

### Phase 3: Quick Validation (30 min)

**Objective**: Verify all builds create valid images

**Test Matrix**:
```bash
# Test data
mkdir -p /tmp/test-data
dd if=/dev/urandom of=/tmp/test-data/file1.bin bs=1M count=10
dd if=/dev/urandom of=/tmp/test-data/file2.bin bs=1M count=10
cp /tmp/test-data/file1.bin /tmp/test-data/file3.bin

# Test each build
for build in fb tb dual; do
  echo "Testing build-$build..."
  
  # Create image
  ./build-$build/mkdwarfs -i /tmp/test-data -o /tmp/$build-test.dwarfs
  
  # Verify image
  ./build-$build/dwarfsck /tmp/$build-test.dwarfs
  
  # Extract using mount+copy workaround
  mkdir -p /tmp/mnt-$build /tmp/extract-$build
  ./build-$build/dwarfs /tmp/$build-test.dwarfs /tmp/mnt-$build &
  sleep 2
  cp -a /tmp/mnt-$build/* /tmp/extract-$build/
  umount /tmp/mnt-$build
  
  # Verify content
  diff -r /tmp/test-data /tmp/extract-$build && echo "✅ $build passed"
done
```

**Success Criteria**:
- All 3 builds create valid images
- All images pass integrity checks
- Extracted content matches original (via mount+copy)
- FlatBuffers images ~102-109% size of Thrift

---

### Phase 4: Simplified Benchmarking (2-3 hours)

**Objective**: Compare format size/performance using mount+copy

**Approach**: Skip dwarfsextract, use FUSE driver instead

**Script**: `benchmarks/run_3build_comparison_mountcopy.py`

```python
#!/usr/bin/env python3
"""
3-Build Comparison using Mount+Copy workaround
"""

import subprocess
import time
from pathlib import Path

def mount_and_copy(dwarfs_bin, image, mount_point, output_dir):
    """Mount image and copy files"""
    # Mount
    proc = subprocess.Popen([dwarfs_bin, image, mount_point])
    time.sleep(2)  # Wait for mount
    
    # Copy
    start = time.time()
    subprocess.run(['cp', '-a', f'{mount_point}/', output_dir], check=True)
    copy_time = time.time() - start
    
    # Unmount
    subprocess.run(['umount', mount_point], check=True)
    proc.wait()
    
    return copy_time

# Run benchmarks...
```

**Metrics to Collect**:
- Image creation time
- Image size (bytes)
- Metadata size (from dwarfsck --json)
- Mount time (optional)
- Copy throughput (substitute for extraction)
- Peak memory usage

---

### Phase 5: Generate Comparison Report (1 hour)

**Output**: `benchmark-results/3BUILD_COMPARISON_REPORT.md`

**Sections**:
1. **Executive Summary**: Format recommendations
2. **Size Comparison**: FlatBuffers vs Thrift actual measurements
3. **Performance**: Creation time, mount+copy throughput
4. **Known Limitations**: dwarfsextract issue documented
5. **Recommendations**: When to use each format

---

### Phase 6: Update Documentation (1 hour)

**Files to Update**:

1. **README.md** - Add metadata format section:
```markdown
## Metadata Formats

DwarFS supports two metadata serialization formats:

- **FlatBuffers** (v24.3.25) - Default, modern format
  - Excellent portability (header-only)
  - ~102-109% size of Thrift (tested)
  - Recommended for new images
  
- **Thrift Compact** (legacy) - For backward compatibility
  - Requires Folly + fbthrift
  - Smallest format
  - Reading old images only
```

2. **doc/CURRENT_STATUS_AND_NEXT_STEPS.md** - Archive completed work

3. **Move old docs**:
```bash
mkdir -p doc/old-docs/dwarfsextract-bug
mv doc/BENCHMARKING_FINDINGS_2025-12-03.md doc/old-docs/dwarfsextract-bug/
mv doc/BENCHMARKING_FIX_ALL_* doc/old-docs/dwarfsextract-bug/
```

---

### Phase 7: Return to dwarfsextract (Future Work)

**Approaches**:

1. **ASAN Build** (2-3 hours):
```bash
cmake -B build-asan -GNinja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DENABLE_ASAN=ON \
  -DDWARFS_WITH_FLATBUFFERS=ON \
  -DDWARFS_WITH_THRIFT=OFF
ninja -C build-asan dwarfsextract

# Run with ASAN
./build-asan/dwarfsextract -i /tmp/minimal.dwarfs -o /tmp/extract
# Will provide detailed crash location
```

2. **Git Bisect** (1-2 hours):
```bash
git bisect start
git bisect bad HEAD
git bisect good <last-known-working-commit>
# Test each commit until regression found
```

3. **Upstream Report** (1 hour):
- File detailed GitHub issue
- Include ASAN output
- Provide minimal reproducer

---

## Timeline

| Phase | Task | Duration | Status |
|-------|------|----------|--------|
| 1 | Debug dwarfsextract | 4h spent | ⏸️ DEFERRED |
| 2 | Create Thrift header | 1-2h | ⏭️ NEXT |
| 3 | Quick validation | 0.5h | ⏸️ Pending |
| 4 | Simplified benchmarking | 2-3h | ⏸️ Pending |
| 5 | Generate report | 1h | ⏸️ Pending |
| 6 | Update documentation | 1h | ⏸️ Pending |
| 7 | Fix dwarfsextract (future) | 3-5h | 🔮 Future |

**Total Remaining**: 5.5-7.5 hours (excluding Phase 7)

---

## Success Criteria

### Minimum Success (Allow Release)
- [x] dwarfsextract bug documented
- [ ] Thrift header created
- [ ] All 3 builds working
- [ ] Basic validation passed (mount+copy)
- [ ] Size comparison completed

### Full Success (Complete Benchmarking)
- [ ] All phases 2-6 complete
- [ ] Comprehensive report generated
- [ ] Documentation updated
- [ ] Known issues documented for release

---

## Deliverables

- [ ] `include/dwarfs/writer/internal/thrift_metadata_builder_impl.h`
- [ ] 3 working build directories (fb, tb, dual)
- [ ] `benchmarks/run_3build_comparison_mountcopy.py`
- [ ] `benchmark-results/3BUILD_COMPARISON_REPORT.md`
- [ ] Updated README.md
- [ ] Archived old documentation
- [x] `doc/DWARFSEXTRACT_BUG_ANALYSIS.md`

---

**Status**: 📋 **READY FOR PHASE 2**  
**Next Action**: Create `thrift_metadata_builder_impl.h`  
**Expected Completion**: 2025-12-04 evening