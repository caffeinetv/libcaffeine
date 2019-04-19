#!/bin/bash

if ! hash clang-format 2>/dev/null; then
    echo "clang-format is missing, please add it to the global path"
    exit 1
fi

files=()
readarray -d '' files_h < <(find include/ -type f -name "*.h" -or -name "*.hpp" -or -name "*.c" -or -name "*.cpp" -print0)
readarray -d '' files_c < <(find src/ -type f -name "*.h" -or -name "*.hpp" -or -name "*.c" -or -name "*.cpp" -print0)
files=("${files_h[@]}" "${files_c[@]}")

for v in "${files[@]}"; do
    clang-format -i "$v"
done

exit 0
