#!/bin/sh
set -eu

# The default unlimited run and explicit zero timeout must both stop cleanly.
for signal in TERM INT; do
    if [ "$signal" = TERM ]; then
        "$1" --maintain-load --quiet &
    else
        "$1" --maintain-load --quiet --timeout=0 &
    fi
    load_pid=$!
    trap 'kill -TERM "$load_pid" 2>/dev/null || true; wait "$load_pid" 2>/dev/null || true' EXIT HUP INT TERM
    sleep 2
    kill "-$signal" "$load_pid"
    wait "$load_pid"
    trap - EXIT HUP INT TERM
done
