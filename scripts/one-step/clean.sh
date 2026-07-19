#!/bin/bash
# One-Step Clean Script for DwarFS
#
# This is a convenience wrapper for scripts/utils/clean.sh
#
# Usage:
#   ./scripts/one-step/clean.sh [--all] [--yes]

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

exec "$PROJECT_ROOT/scripts/utils/clean.sh" "$@"
