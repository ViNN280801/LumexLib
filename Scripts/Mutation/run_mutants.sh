#!/bin/sh
# Builds and runs the mutants of mutants.json against a configured build tree; see README.md.
# Usage: run_mutants.sh --build-dir <dir> --overlay <dir> [--only 'V2/*'] [--layer v1] [--jobs 2] [--configure] ...
# Exit status: 0 every verdict is as expected, 1 a mismatch, 2 a stale pattern, 3 the setup is wrong.
HERE=$(cd "$(dirname "$0")" && pwd)
PYTHON=${PYTHON:-python3}
exec env LC_ALL=C "$PYTHON" "$HERE/run_mutants.py" "$@"
