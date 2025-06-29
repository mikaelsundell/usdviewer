#!/bin/bash
script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
project_root="$(cd "$script_dir/.." && pwd)"

echo "Running clang-format in project root: $project_root"

find "$project_root" -maxdepth 1 -name '*.cpp' -o -name '*.h' | xargs clang-format -i
echo "Clang-format completed."