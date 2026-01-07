# Session 55 Continuation Prompt - UPDATED PRIORITY

**For Next Session**: Start here to address the **CRITICAL BLOCKER** discovered during Week 1 testing.

---

## 🔴 CRITICAL: Homebrew Compatibility Issue

**STATUS**: **BLOCKING ALL FURTHER WORK**

During Week 1 validation testing, we discovered that files created by our development build **CANNOT be read by Homebrew dwarfs 0.14.1**, despite both using Thrift format.

**Read This First**: [`doc/HOMEBREW_COMPATIBILITY_ISSUE.md`](HOMEBREW_COMPATIBILITY_ISSUE.md)

---

## Immediate Priority: Fix Compatibility (5.5-7.5 hours)

Execute the 4-phase investigation and fix plan from the compatibility issue document:

### Phase 1: Baseline Comparison (1 hour)

```bash
cd /Users/mulgogi/src/external/dwarfs

# 1. Checkout v0.14.1 tag
git checkout v0.14.1
mkdir build-v0.14.1
cmake -B build-v0.14.1 -GNinja -DCMAKE_BUILD_TYPE=Release
ninja -C build-v0.14.1

# 2. Test v0.14.1 → Homebrew compatibility
./build-v0.14.1/mkdwarfs --format=thrift -i /tmp/test -o /tmp/v0141.dwarfs -l7
/opt/homebrew/Cellar/dwarfs/0.14.1_3/bin/dwarfsck -i /tmp/v0141.dwarfs
# Expected: ✅ PASS

# 3. Compare Thrift schemas
git diff v0.14.1..feature/multi-format-serialization-fuse -- thrift/metadata.thrift

# Return to feature branch
git checkout feature/multi-format-serialization-fuse
```

### Phase 2: Binary Diff Analysis (30 min)

```bash
# Hex dump comparison
xxd /tmp/homebrew-created.dwarfs > /tmp/homebrew.hex
xxd /tmp/local-created-thrift.dwarfs > /tmp/local.hex
diff -u /tmp/homebrew.hex /tmp/local.hex | head -100

# Metadata JSON export
./build/dwarfsck --export-metadata=/tmp/homebrew-meta.json \
    -i /tmp/homebrew-created.dwarfs
./build/dwarfsck --export-metadata=/tmp/local-meta.json \
    -i /tmp/local-created-thrift.dwarfs
diff -u /tmp/homebrew-meta.json /tmp/local-meta.json
```

### Phase 3: Code Analysis (1 hour)

```bash
# Review commits affecting Thrift
git log v0.14.1..HEAD --oneline -- \
    thrift/ \
    src/metadata/ \
    src/reader/internal/metadata_v2_thrift.cpp \
    src/writer/internal/metadata_builder.cpp

# Identify breaking changes in each commit
# Focus on: schema modifications, serialization logic, packing options
```

### Phase 4: Fix Implementation (2-4 hours)

Based on Phase 3 findings, choose strategy:

**Option A**: Revert specific breaking commits
**Option B**: Add v0.14.1 compatibility mode
**Option C**: Bump to v0.15.0 (document breaking change)

---

## Success Criteria

✅ **BLOCKER RESOLVED** when:
1. Our build creates Thrift files readable by Homebrew 0.14.1
2. Homebrew compatibility test passes:
   ```bash
   # Create with our build
   ./build/mkdwarfs --format=thrift -i /tmp/test -o /tmp/compat-test.dwarfs -l7

   # Read with Homebrew
   /opt/homebrew/Cellar/dwarfs/0.14.1_3/bin/dwarfsck -i /tmp/compat-test.dwarfs
   /opt/homebrew/Cellar/dwarfs/0.14.1_3/bin/dwarfsextract \
       -i /tmp/compat-test.dwarfs -o /tmp/extracted

   # Verify content
   diff -r /tmp/test /tmp/extracted
   ```
3. Regression test added to prevent future breaks

---

## ONLY After Compatibility Fixed

Resume original Session 55 plan:

### Week 1 Remaining
- Complete Task 4 full vcpkg run (2-3 hours)

### Week 2: MECE Compliance
- Task 5: Clean up scripts/ and benchmarks/ (4 hours)

### Week 3: Homebrew Compatibility (NOW COMPLETE)
- ~~Task 6: Verify compatibility~~ ✅ Now part of blocker fix

### Week 4: Dependency Devendoring
- Task 7: Remove 150MB+ vendored dependencies (8-12 hours)

---

## Reference Documents

**CRITICAL**:
- [`doc/HOMEBREW_COMPATIBILITY_ISSUE.md`](HOMEBREW_COMPATIBILITY_ISSUE.md) - Investigation plan

**Supporting**:
- [`doc/SESSION_55_IMPLEMENTATION_STATUS.md`](SESSION_55_IMPLEMENTATION_STATUS.md) - Progress tracker
- [`doc/SESSION_55_CONTINUATION_PLAN.md`](SESSION_55_CONTINUATION_PLAN.md) - Original 4-week plan

---

**Current Priority**: Fix Homebrew compatibility BEFORE any other work
**Estimated Time**: 5.5-7.5 hours
**Blocker Severity**: CRITICAL - blocks production use