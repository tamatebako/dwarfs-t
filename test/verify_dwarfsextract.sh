#!/bin/bash
set -e

echo "=== DwarFS Extract Verification Suite ==="

# Get absolute path to project root
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

BUILD_DIR="$PROJECT_ROOT/${1:-build-fb}"
WORK_DIR="/tmp/dwarfs-extract-test"

# Cleanup and setup
rm -rf "$WORK_DIR"
mkdir -p "$WORK_DIR"
cd "$WORK_DIR"

# Color output
GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m' # No Color

test_passed=0
test_failed=0

run_test() {
    local test_name="$1"
    local src_dir="$2"
    local img_name="$3"
    
    echo ""
    echo "Test: $test_name"
    echo "  Creating image..."
    "$BUILD_DIR/mkdwarfs" -i "$src_dir" -o "$img_name"
    
    echo "  Extracting..."
    local extract_dir="${img_name%.dwarfs}-extracted"
    "$BUILD_DIR/dwarfsextract" -i "$img_name" -o "$extract_dir"
    
    echo "  Verifying..."
    if diff -r "$src_dir" "$extract_dir"; then
        echo -e "${GREEN}✅ Test passed: $test_name${NC}"
        ((test_passed++))
    else
        echo -e "${RED}❌ Test failed: $test_name${NC}"
        ((test_failed++))
        return 1
    fi
}

# Test 1: Single file
echo "=== Test 1: Single file ==="
mkdir -p t1
echo "hello world" > t1/file.txt
run_test "Single file" t1 t1.dwarfs

# Test 2: Multiple files with subdirectories
echo "=== Test 2: Multiple files with subdirectories ==="
mkdir -p t2/{a,b/c}
echo "1" > t2/a/1.txt
echo "2" > t2/b/2.txt
echo "3" > t2/b/c/3.txt
run_test "Multiple files" t2 t2.dwarfs

# Test 3: Large file (10 MB)
echo "=== Test 3: Large file ==="
mkdir -p t3
dd if=/dev/urandom of=t3/large.bin bs=1M count=10 2>/dev/null
run_test "Large file" t3 t3.dwarfs

# Test 4: Duplicate files (deduplication)
echo "=== Test 4: Duplicate files ==="
mkdir -p t4
echo "duplicate content" > t4/a.txt
cp t4/a.txt t4/b.txt
cp t4/a.txt t4/c.txt
run_test "Duplicate files" t4 t4.dwarfs

# Test 5: Special characters in names
echo "=== Test 5: Special characters in names ==="
mkdir -p t5
touch "t5/file with spaces.txt"
touch "t5/file-with-dashes.txt"
touch "t5/file_with_underscores.txt"
echo "content" > "t5/file with spaces.txt"
echo "content" > "t5/file-with-dashes.txt"
echo "content" > "t5/file_with_underscores.txt"
run_test "Special characters" t5 t5.dwarfs

# Test 6: Empty files
echo "=== Test 6: Empty files ==="
mkdir -p t6
touch t6/empty1.txt
touch t6/empty2.txt
mkdir -p t6/subdir
touch t6/subdir/empty3.txt
run_test "Empty files" t6 t6.dwarfs

# Test 7: Deep directory structure
echo "=== Test 7: Deep directory structure ==="
mkdir -p t7/a/b/c/d/e/f/g/h
echo "deep file" > t7/a/b/c/d/e/f/g/h/file.txt
run_test "Deep directories" t7 t7.dwarfs

# Summary
echo ""
echo "========================================="
echo "Test Summary:"
echo -e "${GREEN}Passed: $test_passed${NC}"
if [ $test_failed -gt 0 ]; then
    echo -e "${RED}Failed: $test_failed${NC}"
    exit 1
else
    echo -e "${GREEN}All tests passed!${NC}"
fi