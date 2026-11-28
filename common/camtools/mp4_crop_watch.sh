#!/system/bin/sh
# mp4_crop_watch.sh: rewrite the SPS of freshly recorded camera videos so the
# decoder crops the 16 green rows the 6.0.1 camera daemon leaves at the bottom
# of every frame.
#
# A file is only touched after its size has stayed the same across two polls,
# so recordings that are still being written are never patched in the middle.
# Already-processed files are remembered in $DONE and never read again.

STATE=/data/local/tmp/mp4_crop.state
DONE=/data/local/tmp/mp4_crop.done
mkdir -p /data/local/tmp
[ -f "$STATE" ] || : > "$STATE"
[ -f "$DONE" ] || : > "$DONE"

while true; do
    : > "$STATE.new"
    for f in /sdcard/DCIM/Camera/*.mp4; do
        [ -f "$f" ] || continue
        set -- $(stat -c '%s %Y' "$f" 2>/dev/null)
        sz=$1; mt=$2
        [ -n "$sz" ] || continue
        grep -qxF "$f:$sz" "$DONE" && continue
        prev=$(awk -v p="$f" '$3==p {print $1, $2; exit}' "$STATE")
        if [ "$prev" = "$sz $mt" ]; then
            /system/vendor/bin/sps_crop "$f" >/dev/null 2>&1
            # drop the stale thumbnail that still shows the green band
            /system/bin/am broadcast -a android.intent.action.MEDIA_SCANNER_SCAN_FILE \
                -d "file://$f" >/dev/null 2>&1
            echo "$f:$(stat -c '%s' "$f" 2>/dev/null)" >> "$DONE"
        else
            echo "$sz $mt $f" >> "$STATE.new"
        fi
    done
    mv "$STATE.new" "$STATE"
    sleep 1
done
