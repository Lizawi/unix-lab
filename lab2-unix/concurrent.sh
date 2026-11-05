#!/bin/sh

DIR=/data
ID=$(od -An -N8 -tx1 /dev/urandom | tr -d ' \n')
N=0
NAME=

trap '[ -n "$NAME" ] && rm -f "$DIR/$NAME"; exit 0' INT TERM

while true; do
    NAME=$(
        exec 9>"$DIR/.lock"
        flock 9 || exit 1
        i=1
        while [ -e "$DIR/$(printf '%03d' "$i")" ]; do i=$((i + 1)); done
        F=$(printf '%03d' "$i")
        printf '%s\n%s\n' "$ID" "$((N + 1))" > "$DIR/$F" && echo "$F"
    )
    [ -n "$NAME" ] || { sleep 1; continue; }
    N=$((N + 1))
    sleep 1
    rm -f "$DIR/$NAME"
    NAME=
    sleep 1
done
