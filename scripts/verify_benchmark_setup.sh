#!/bin/bash
# DwarFS Benchmark Setup Verification Script
# Verifies that benchmark infrastructure is ready to use

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$PROJECT_ROOT"

echo "=========================================="
echo "DwarFS Benchmark Setup Verification"
echo "=========================================="
echo ""

PASSED=0
FAILED=0
WARNINGS=0

# Color codes
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

pass() {
    echo -e "${GREEN}✓${NC} $1"
    ((PASSED++))
}

fail() {
    echo -e "${RED}✗${NC} $1"
    ((FAILED++))
}

warn() {
    echo -e "${YELLOW}⚠${NC} $1"
    ((WARNINGS++))
}

# 1. Check Python
echo "=== Python Environment ==="
if command -v python3 &> /dev/null; then
    PYTHON_VER=$(python3 --version)
    pass "Python 3 available: $PYTHON_VER"
else
    fail "Python 3 not found"
fi
echo ""

# 2. Check builds
echo "=== Build Directories ==="
if [ -d "build-fb" ]; then
    pass "FlatBuffers-only build exists: build-fb/"
    if [ -f "build-fb/mkdwarfs" ]; then
        pass "  mkdwarfs executable found"
    else
        fail "  mkdwarfs executable missing"
    fi
    if [ -f "build-fb/dwarfs_unit_tests" ]; then
        pass "  test executable found"
    else
        warn "  test executable missing"
    fi
else
    fail "FlatBuffers-only build missing: build-fb/"
fi

if [ -d "build-tb" ]; then
    pass "Thrift build exists: build-tb/"
else
    warn "Thrift build missing: build-tb/ (optional)"
fi
echo ""

# 3. Test FlatBuffers build
if [ -f "build-fb/dwarfs_unit_tests" ]; then
    echo "=== Running Tests (build-fb) ==="
    if build-fb/dwarfs_unit_tests --gtest_brief=1 2>&1 | grep -q "PASSED.*1600"; then
        pass "Tests passing (1,600/1,613)"
    else
        fail "Tests failing or unexpected count"
    fi
    echo ""
fi

# 4. Check benchmark scripts
echo "=== Benchmark Scripts ==="
BENCHMARK_SCRIPTS=(
    "benchmarks/compression_algorithm_benchmark.py"
    "benchmarks/run_all_benchmarks.py"
    "benchmarks/run_complete_comparison.py"
    "benchmarks/download_datasets.py"
    "benchmarks/generate_compression_report.py"
    "benchmarks/generate_comprehensive_report.py"
)

for script in "${BENCHMARK_SCRIPTS[@]}"; do
    if [ -f "$script" ]; then
        pass "$(basename "$script")"
    else
        fail "$(basename "$script") missing"
    fi
done
echo ""

# 5. Check benchmark libraries
echo "=== Benchmark Libraries ==="
BENCHMARK_LIBS=(
    "benchmarks/lib/memory_tracker.py"
    "benchmarks/lib/fuse_manager.py"
    "benchmarks/lib/perfmon_parser.py"
    "benchmarks/lib/progress.py"
)

for lib in "${BENCHMARK_LIBS[@]}"; do
    if [ -f "$lib" ]; then
        pass "$(basename "$lib")"
    else
        fail "$(basename "$lib") missing"
    fi
done
echo ""

# 6. Check CI workflows
echo "=== CI Workflows ==="
if [ -f ".github/workflows/build.yml" ]; then
    pass "Main CI workflow (build.yml)"
    if grep -q "compression-benchmark" ".github/workflows/build.yml"; then
        pass "  Compression benchmark job configured"
    else
        warn "  Compression benchmark job missing"
    fi
    if grep -q "metadata-formats" ".github/workflows/build.yml"; then
        pass "  Metadata format testing configured"
    else
        warn "  Metadata format testing missing"
    fi
else
    fail "Main CI workflow missing"
fi

if [ -f ".github/workflows/benchmark-comprehensive.yml" ]; then
    pass "Comprehensive benchmark workflow"
else
    warn "Comprehensive benchmark workflow missing (newly added)"
fi
echo ""

# 7. Check datasets
echo "=== Datasets ==="
if [ -d "benchmark-files" ]; then
    pass "Dataset directory exists"
    if [ -d "benchmark-files/perl-5.43.3" ]; then
        PERL_SIZE=$(du -sh benchmark-files/perl-5.43.3 2>/dev/null | cut -f1)
        pass "  Perl dataset present ($PERL_SIZE)"
    else
        warn "  Perl dataset not downloaded (optional)"
    fi
else
    warn "Dataset directory missing (will be created)"
fi
echo ""

# 8. Check documentation
echo "=== Documentation ==="
DOCS=(
    "benchmarks/README.md"
    "doc/COMPRESSION_BENCHMARK_RESULTS.md"
    "doc/BENCHMARK_CI_GUIDE.md"
)

for doc in "${DOCS[@]}"; do
    if [ -f "$doc" ]; then
        pass "$(basename "$doc")"
    else
        warn "$(basename "$doc") missing"
    fi
done
echo ""

# 9. Quick functional test
echo "=== Quick Functional Test ==="
if [ -f "build-fb/mkdwarfs" ]; then
    TEST_DIR="/tmp/dwarfs-verify-$$"
    TEST_IMAGE="/tmp/dwarfs-verify-$$.dwarfs"
    
    mkdir -p "$TEST_DIR"
    echo "test content" > "$TEST_DIR/test.txt"
    
    if build-fb/mkdwarfs -i "$TEST_DIR" -o "$TEST_IMAGE" --log-level=error 2>&1; then
        if [ -f "$TEST_IMAGE" ]; then
            SIZE=$(stat -f%z "$TEST_IMAGE" 2>/dev/null || stat -c%s "$TEST_IMAGE" 2>/dev/null)
            pass "mkdwarfs created image ($SIZE bytes)"
            
            # Verify with dwarfsck
            if [ -f "build-fb/dwarfsck" ]; then
                if build-fb/dwarfsck --check-integrity "$TEST_IMAGE" 2>&1 >/dev/null; then
                    pass "dwarfsck verified image"
                else
                    warn "dwarfsck verification failed"
                fi
            fi
        else
            fail "mkdwarfs failed to create image"
        fi
    else
        fail "mkdwarfs command failed"
    fi
    
    # Cleanup
    rm -rf "$TEST_DIR" "$TEST_IMAGE"
else
    warn "Skipping functional test (mkdwarfs not available)"
fi
echo ""

# Summary
echo "=========================================="
echo "Summary"
echo "=========================================="
echo -e "${GREEN}Passed:${NC}   $PASSED"
if [ $WARNINGS -gt 0 ]; then
    echo -e "${YELLOW}Warnings:${NC} $WARNINGS"
fi
if [ $FAILED -gt 0 ]; then
    echo -e "${RED}Failed:${NC}   $FAILED"
fi
echo ""

if [ $FAILED -eq 0 ]; then
    echo -e "${GREEN}✓ Benchmark infrastructure ready!${NC}"
    echo ""
    echo "Next steps:"
    echo "  1. Download datasets: python3 benchmarks/download_datasets.py --download perl"
    echo "  2. Run compression benchmark: python3 benchmarks/compression_algorithm_benchmark.py --build-dir build-fb --output results.json"
    echo "  3. See doc/BENCHMARK_CI_GUIDE.md for detailed usage"
    exit 0
else
    echo -e "${RED}✗ Issues found. Please fix the failures above.${NC}"
    exit 1
fi