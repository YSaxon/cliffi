#!/bin/bash

# Check if at least one argument is provided
if [ "$#" -lt 1 ]; then
    echo "Usage: $0 <command> [<args to pass as STDIN>...]"
    exit 1
fi

# The first argument is the command
COMMAND=$1

# Remove the first argument from the list
shift

(for ARG in "$@"; do
    echo -n "$ARG "
done ) | $COMMAND --repl

