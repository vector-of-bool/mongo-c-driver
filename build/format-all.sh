#!/usr/bin/env bash

set -euo pipefail
. "$(dirname "${BASH_SOURCE[0]}")/../.evergreen/scripts/env-var-utils.sh"

root_dir="$(to_absolute "$(dirname "${BASH_SOURCE[0]}")/..")"

files="$(find "$root_dir/src"/{libmongoc,libbson,kms-message,common,tools} -type f -name '*.c' -o -name '*.h' -o -name '*.cpp' -o -name '*.hpp')"
# shellcheck disable=SC2206
IFS=$'\n' files=($files)
bash "$root_dir/build/format.sh" \
    --style="file:$root_dir/.clang-format" \
    -i \
    "$@" \
    -- "${files[@]:?}"
