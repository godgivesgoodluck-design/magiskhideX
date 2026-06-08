#!/system/bin/sh
# MagiskHideX - service.sh
# Runs after boot_completed, keeps module health in check

DATADIR="/data/adb/magiskhidex"
LOGFILE="$DATADIR/magiskhidex.log"
WHITELIST_FILE="$DATADIR/whitelist"
UMOUNT_FILE="$DATADIR/umount_list"

log_msg() {
    echo "[$(date '+%H:%M:%S')] [service] $1" >> "$LOGFILE"
}

# Wait until boot is complete
while [ "$(getprop sys.boot_completed)" != "1" ]; do
    sleep 1
done

sleep 3  # Give system processes time to settle

log_msg "=== MagiskHideX Service Started ==="
log_msg "Magisk: $MAGISK_VER_CODE | Zygisk: $ZYGISK_ENABLED"

# ─── Mode Detection ──────────────────────────────────────────────────
if [ -f "$WHITELIST_FILE" ]; then
    log_msg "Mode: WHITELIST (only root-granted apps can access root)"
else
    log_msg "Mode: DENYLIST (apps in DenyList will have root hidden)"
fi

# ─── NeoZygisk Detection ─────────────────────────────────────────────
NEOZYGISK_ACTIVE=0
if [ -d "/data/adb/modules/neozygisk" ] && [ ! -f "/data/adb/modules/neozygisk/disable" ]; then
    NEOZYGISK_ACTIVE=1
    log_msg "NeoZygisk: ACTIVE"
elif [ -d "/data/adb/modules/zygisksu" ] && [ ! -f "/data/adb/modules/zygisksu/disable" ]; then
    NEOZYGISK_ACTIVE=1
    log_msg "ZygiskNext: ACTIVE"
else
    log_msg "NeoZygisk: not found, using built-in Zygisk"
fi

# ─── Log DenyList Apps ───────────────────────────────────────────────
log_msg "Current DenyList apps:"
magisk --denylist ls 2>/dev/null | while read -r line; do
    log_msg "  -> $line"
done

# ─── Trim log if too large (>500KB) ─────────────────────────────────
LOG_SIZE=$(wc -c < "$LOGFILE" 2>/dev/null || echo 0)
if [ "$LOG_SIZE" -gt 512000 ]; then
    tail -n 500 "$LOGFILE" > "${LOGFILE}.tmp"
    mv "${LOGFILE}.tmp" "$LOGFILE"
    log_msg "Log trimmed."
fi

log_msg "Service initialization complete."
