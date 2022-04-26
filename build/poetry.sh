#!/bin/env bash

set -euo pipefail

mcd_dir="$HOME/.cache/mongo-c-build"
mkdir -p "$mcd_dir"

poetry_dir="$mcd_dir/poetry"
export POETRY_HOME="$poetry_dir"
export PATH="$POETRY_HOME/bin:$PATH"

poetry="$POETRY_HOME/bin/poetry"

if ! test -f "$poetry"; then
    echo "Note: Installing Poetry in [$POETRY_HOME]"
    : "${PYTHON:=python3}"
    get_poetry_py="$mcd_dir/get-poetry.py"
    curl -sL https://install.python-poetry.org -o "$get_poetry_py" \
    || wget https://install.python-poetry.org -qO "$get_poetry_py"
    "$PYTHON" -u "$get_poetry_py" --yes
fi

if ! test -f "$poetry"; then
    echo "Failed to install Poetry" 1>&2
    exit 1
fi

"$poetry" "$@"
