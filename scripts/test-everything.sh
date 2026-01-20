#!/bin/bash
# Backward compatibility wrapper for test-everything.sh
# The script has moved to scripts/one-step/test-everything.sh

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

exec "$PROJECT_ROOT/scripts/one-step/test-everything.sh" "$@"
