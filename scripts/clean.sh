#!/bin/bash
# Backward compatibility wrapper for clean.sh
# The new unified clean script is at scripts/utils/clean.sh

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

exec "$PROJECT_ROOT/scripts/utils/clean.sh" "$@"
