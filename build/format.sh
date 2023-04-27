#!/usr/bin/env bash

set -euo pipefail
. "$(dirname "${BASH_SOURCE[0]}")/../.evergreen/scripts/env-var-utils.sh"

: "${CLANG_FORMAT_VERSION:=16.0.2}"

if ! python3 -c ''; then
  fail "No Python found?"
fi

# Check that we have a pipx of the proper version:
python3 -c 'import pkg_resources; pkg_resources.require("pipx>=0.17.0<2.0")'

# Give default clang-format an empty string on stdin if there are no inputs files
printf '' | python3 -m pipx run "clang-format==${CLANG_FORMAT_VERSION:?}" "$@"
