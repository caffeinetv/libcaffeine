#!/bin/bash

# Script to install git hooks

readarray -d '' hooks < <(find .hooks/ -name "*" -type f -print0)

for v in "${hooks[@]}"; do
    if [ -f ".git/hooks/`basename -s .sh $v`" ]; then
        rm ".git/hooks/`basename -s .sh $v`"
    fi
    ln -s "$v" ".git/hooks/`basename -s .sh $v`"
done
