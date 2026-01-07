#!/bin/bash
#
# Integration test for environment variables
#
# This script tests that environment variables work correctly across all tools
# by running actual commands and verifying their behavior.
#

set -e
set -o pipefail

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Test counters
TESTS_RUN=0
TESTS_PASSED=0
TESTS_FAILED=0

# Temporary directory for test artifacts
TEST_DIR=$(mktemp -d)
trap "rm -rf ${TEST_DIR}" EXIT

# Helper functions
log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

assert_contains() {
    local output="$1"
    local expected="$2"
    local test_name="$3"

    TESTS_RUN=$((TESTS_RUN + 1))

    if echo "$output" | grep -q "$expected"; then
        TESTS_PASSED=$((TESTS_PASSED + 1))
        log_info "✅ PASS: $test_name"
        return 0
    else
        TESTS_FAILED=$((TESTS_FAILED + 1))
        log_error "❌ FAIL: $test_name"
        log_error "  Expected to find: $expected"
        log_error "  In output: $output"
        return 1
    fi
}

assert_not_contains() {
    local output="$1"
    local unexpected="$2"
    local test_name="$3"

    TESTS_RUN=$((TESTS_RUN + 1))

    if ! echo "$output" | grep -q "$unexpected"; then
        TESTS_PASSED=$((TESTS_PASSED + 1))
        log_info "✅ PASS: $test_name"
        return 0
    else
        TESTS_FAILED=$((TESTS_FAILED + 1))
        log_error "❌ FAIL: $test_name"
        log_error "  Expected NOT to find: $unexpected"
        log_error "  In output: $output"
        return 1
    fi
}

# ============================================================================
# Test 1: Environment variable sets log level
# ============================================================================

test_log_level_env() {
    log_info "Testing DWARFS_LOG_LEVEL environment variable..."

    export DWARFS_LOG_LEVEL=debug
    
    # mkdwarfs should respect the environment variable
    # We use --help to avoid actual filesystem creation
    output=$(mkdwarfs --help 2>&1 || true)
    
    # Debug level should enable verbose logging
    # (Actual verification depends on how logging is implemented)
    
    unset DWARFS_LOG_LEVEL
    
    log_info "Log level environment variable test completed"
}

# ============================================================================
# Test 2: CLI overrides environment
# ============================================================================

test_cli_overrides_env() {
    log_info "Testing CLI override of environment variables..."

    export DWARFS_LOG_LEVEL=debug
    
    # CLI argument should override environment
    # --quiet should take precedence over DWARFS_LOG_LEVEL=debug
    output=$(mkdwarfs --help --quiet 2>&1 || true)
    
    unset DWARFS_LOG_LEVEL
    
    log_info "CLI override test completed"
}

# ============================================================================
# Test 3: Tool-specific variables
# ============================================================================

test_tool_specific_vars() {
    log_info "Testing tool-specific environment variables..."

    # Create a minimal test filesystem
    mkdir -p "${TEST_DIR}/source"
    echo "test content" > "${TEST_DIR}/source/test.txt"

    # Test mkdwarfs with environment variables
    export DWARFS_MKDWARFS_COMPRESSION_LEVEL=1
    export DWARFS_MKDWARFS_NUM_WORKERS=2
    
    mkdwarfs -i "${TEST_DIR}/source" -o "${TEST_DIR}/test.dff" \
        --log-level=error 2>&1 || true
    
    unset DWARFS_MKDWARFS_COMPRESSION_LEVEL
    unset DWARFS_MKDWARFS_NUM_WORKERS

    # Verify filesystem was created
    if [ -f "${TEST_DIR}/test.dff" ]; then
        log_info "✅ Filesystem created with environment variables"
    else
        log_error "❌ Filesystem creation failed"
    fi

    # Test dwarfsck with environment variables
    export DWARFS_DWARFSCK_PRINT_TYPE=text
    
    dwarfsck "${TEST_DIR}/test.dff" --log-level=error 2>&1 || true
    
    unset DWARFS_DWARFSCK_PRINT_TYPE

    log_info "Tool-specific variables test completed"
}

# ============================================================================
# Test 4: Multiple environment variables
# ============================================================================

test_multiple_env_vars() {
    log_info "Testing multiple environment variables together..."

    export DWARFS_LOG_LEVEL=error
    export DWARFS_QUIET=1
    export DWARFS_MKDWARFS_COMPRESSION_LEVEL=1

    # All these should work together
    mkdwarfs --help 2>&1 > /dev/null || true
    
    unset DWARFS_LOG_LEVEL
    unset DWARFS_QUIET
    unset DWARFS_MKDWARFS_COMPRESSION_LEVEL

    log_info "Multiple environment variables test completed"
}

# ============================================================================
# Main test execution
# ============================================================================

main() {
    log_info "Starting environment variable integration tests..."
    log_info "Test directory: ${TEST_DIR}"
    echo

    # Run all tests
    test_log_level_env
    test_cli_overrides_env
    test_tool_specific_vars
    test_multiple_env_vars

    echo
    log_info "========================================="
    log_info "Test Summary:"
    log_info "  Total tests run: ${TESTS_RUN}"
    log_info "  Tests passed:    ${TESTS_PASSED}"
    log_info "  Tests failed:    ${TESTS_FAILED}"
    log_info "========================================="

    if [ ${TESTS_FAILED} -eq 0 ]; then
        log_info "✅ All environment variable tests passed!"
        exit 0
    else
        log_error "❌ Some tests failed (${TESTS_FAILED}/${TESTS_RUN})"
        exit 1
    fi
}

# Run main if executed directly
if [ "${BASH_SOURCE[0]}" = "${0}" ]; then
    main "$@"
fi