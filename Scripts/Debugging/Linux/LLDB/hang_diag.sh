#!/usr/bin/env bash
# Backward-compatible entry point — delegates to universal Scripts/hang_diag.sh
exec "$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)/hang_diag.sh" --lldb "$@"
