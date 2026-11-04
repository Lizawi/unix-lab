#!/bin/sh

DIR=/data
LOCK="$DIR/.lock"

ID=$(od -An -N8 -tx1 /dev/urandom | tr -d ' \n')
N=0

while true; do
    NAME=$(
        (
            flock 9
            i=1
            while [ -e "$DIR/$(printf '%03d' "$i")" ]; do
                i=$((i + 1))
            done
            NAME=$(printf '%03d' "$i")
            : > "$DIR/$NAME"
            echo "$NAME"
        ) 9>"$LOCK"
    )

    N=$((N + 1))
    printf '%s\n%s\n' "$ID" "$N" > "$DIR/$NAME"

    sleep 1

    rm -f "$DIR/$NAME"

    sleep 1
done
