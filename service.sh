#!/system/bin/sh
# MagiskHideX - service.sh

DATADIR="/data/adb/magiskhidex"
LOGFILE="$DATADIR/magiskhidex.log"

mkdir -p "$DATADIR"

log_msg() { echo "[$(date '+%H:%M:%S')] [service] $1" >> "$LOGFILE"; }

# Wait for boot
while [ "$(getprop sys.boot_completed)" != "1" ]; do sleep 1; done
sleep 3

log_msg "=== MagiskHideX Service Started ==="

# ─── Hide Magisk props using resetprop ───────────────────────────────
# These props are checked by root detection apps
MAGISK_PROPS="
ro.magisk.hide
ro.boot.magisk
ro.debuggable
ro.secure
"
log_msg "Hiding Magisk props..."
for prop in $MAGISK_PROPS; do
    val=$(getprop "$prop" 2>/dev/null)
    if [ -n "$val" ]; then
        resetprop --delete "$prop" 2>/dev/null && \
            log_msg "  Deleted prop: $prop=$val" || \
            log_msg "  Could not delete: $prop"
    fi
done

# Ensure ro.debuggable=0 and ro.secure=1 (detection apps check these)
resetprop ro.debuggable 0 2>/dev/null
resetprop ro.secure 1 2>/dev/null
log_msg "Set ro.debuggable=0, ro.secure=1"

# ─── Hide Magisk manager package name from props ──────────────────────
resetprop --delete ro.magisk.version 2>/dev/null

# ─── NeoZygisk Detection ──────────────────────────────────────────────
if [ -d "/data/adb/modules/neozygisk" ] && [ ! -f "/data/adb/modules/neozygisk/disable" ]; then
    log_msg "NeoZygisk: ACTIVE"
elif [ -d "/data/adb/modules/zygisksu" ] && [ ! -f "/data/adb/modules/zygisksu/disable" ]; then
    log_msg "ZygiskNext: ACTIVE"
fi

# ─── Log DenyList ─────────────────────────────────────────────────────
log_msg "DenyList apps:"
magisk --denylist ls 2>/dev/null | while read -r line; do
    log_msg "  -> $line"
done

# ─── Trim log ────────────────────────────────────────────────────────
LOG_SIZE=$(wc -c < "$LOGFILE" 2>/dev/null || echo 0)
if [ "$LOG_SIZE" -gt 512000 ]; then
    tail -n 500 "$LOGFILE" > "${LOGFILE}.tmp" && mv "${LOGFILE}.tmp" "$LOGFILE"
fi

log_msg "Service done."
