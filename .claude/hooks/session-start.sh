#!/bin/bash
# Cloud sessions: install what the whole-program port build needs
# (port/tools/whole_build.sh); the package list is in port/tools/whole_setup_linux.sh,
# which also sets up WSL2 on the Windows PC.
set -euo pipefail

if [ "${CLAUDE_CODE_REMOTE:-}" != "true" ]; then
  exit 0
fi
exec bash "$(dirname "$0")/../../port/tools/whole_setup_linux.sh"
