#!/bin/bash
# Test script for static-site-server

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SERVER="${SCRIPT_DIR}/build/static-site-server"
PORT=8080
BASE_URL="http://localhost:${PORT}"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo "=== Static Site Server Tests ==="
echo ""

# Check prerequisites
if [ ! -f "${SERVER}" ]; then
  echo -e "${RED}ERROR: Server not built. Run ./build.sh first${NC}"
  exit 1
fi

# Global test counters
TOTAL_TESTS_RUN=0
TOTAL_TESTS_PASSED=0

# Helper function to run test
run_test() {
  local name="$1"
  local url="$2"
  local expected_status="$3"
  local check_content="$4"

  TOTAL_TESTS_RUN=$((TOTAL_TESTS_RUN + 1))
  echo -n "  Test ${TOTAL_TESTS_RUN}: ${name}... "

  # Make request
  response=$(curl -s -w "\n%{http_code}" "${BASE_URL}${url}" 2>/dev/null || echo "ERROR")

  if [ "$response" = "ERROR" ]; then
    echo -e "${RED}FAIL (connection error)${NC}"
    return 1
  fi

  # Extract status code (last line)
  status_code=$(echo "$response" | tail -n 1)
  content=$(echo "$response" | sed '$d')

  if [ "$status_code" != "$expected_status" ]; then
    echo -e "${RED}FAIL (expected ${expected_status}, got ${status_code})${NC}"
    return 1
  fi

  if [ -n "$check_content" ]; then
    if ! grep -q "$check_content" <<< "$content"; then
      echo -e "${RED}FAIL (content check failed)${NC}"
      return 1
    fi
  fi

  echo -e "${GREEN}PASS${NC}"
  TOTAL_TESTS_PASSED=$((TOTAL_TESTS_PASSED + 1))
  return 0
}

# Function to test a specific image
test_image() {
  local image_path="$1"
  local image_name="$2"
  local main_page="$3"
  local cover_image="$4"
  local test_image="$5"
  local content_check="$6"
  local test_redirect="$7"  # "yes" or "no"

  echo ""
  echo "Testing ${image_name}..."
  echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

  if [ ! -f "${image_path}" ]; then
    echo -e "${RED}ERROR: Image not found: ${image_path}${NC}"
    return 1
  fi

  # Start server in background
  echo "  Starting server on port ${PORT}..."
  "${SERVER}" --image "${image_path}" --port "${PORT}" &
  SERVER_PID=$!

  # Wait for server to start
  sleep 2

  # Check if server is running
  if ! kill -0 "${SERVER_PID}" 2>/dev/null; then
    echo -e "${RED}  ERROR: Server failed to start${NC}"
    return 1
  fi

  echo -e "  ${GREEN}Server started (PID: ${SERVER_PID})${NC}"
  echo ""

  # Run tests for this image
  if [ "$test_redirect" = "yes" ]; then
    run_test "GET / (redirect to ${main_page})" "/" "200" "${content_check}"
  fi
  run_test "GET ${main_page}" "${main_page}" "200" "${content_check}"
  run_test "GET ${cover_image}" "${cover_image}" "200" ""
  run_test "GET ${test_image}" "${test_image}" "200" ""
  run_test "GET /missing.html (404)" "/missing.html" "404" "not found"

  # Stop server
  if kill -0 "${SERVER_PID}" 2>/dev/null; then
    echo ""
    echo "  Stopping server..."
    kill "${SERVER_PID}" 2>/dev/null || true
    wait "${SERVER_PID}" 2>/dev/null || true
  fi

  echo ""
}

# Test Aesop's Fables (redirect works with auto-detection)
test_image \
  "${SCRIPT_DIR}/aesop.dff" \
  "Aesop's Fables (pg11339)" \
  "/pg11339-images.html" \
  "/11339-cover.png" \
  "/images/01hare.jpg" \
  "Aesop" \
  "yes"

# Test Candide (redirect works with auto-detection)
test_image \
  "${SCRIPT_DIR}/candide.dff" \
  "Candide (pg19942)" \
  "/pg19942-images.html" \
  "/19942-cover.png" \
  "/images/001.jpg" \
  "Candide" \
  "yes"

echo ""
echo "═══════════════════════════════════════════"
echo "=== Overall Test Summary ==="
echo "═══════════════════════════════════════════"
echo "Total tests run: ${TOTAL_TESTS_RUN}"
echo "Total tests passed: ${TOTAL_TESTS_PASSED}"

if [ "${TOTAL_TESTS_PASSED}" -eq "${TOTAL_TESTS_RUN}" ]; then
  echo -e "${GREEN}All tests passed!${NC}"
  exit 0
else
  echo -e "${RED}Some tests failed${NC}"
  exit 1
fi