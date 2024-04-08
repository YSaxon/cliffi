# Check if at least one argument is provided
if [ "$#" -lt 1 ]; then
    echo "Usage: $0 <command> [<args to pass as STDIN>...]"
    exit 1
fi

# The first argument is the command
COMMAND=$1

# Remove the first argument from the list
shift

#if there's a var QEMU_SET_ENV, set it
if [ -n "$QEMU_SET_ENV" ]; then
    export $QEMU_SET_ENV
fi

(for ARG in "$@"; do
    echo -n "$ARG "
done ) | $COMMAND --repl || { echo "Error: $COMMAND failed."; file $COMMAND; env; exit 1;}
