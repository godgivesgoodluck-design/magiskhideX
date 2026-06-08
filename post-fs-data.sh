#!/system/bin/sh
# MagiskHideX - post-fs-data.sh
# Runs early in boot, before most processes start

DATADIR="/data/adb/magiskhidex"
LOGFILE="$DATADIR/magiskhidex.log"

mkdir -p "$DATADIR"

log_msg() {
    echo "[$(date '+%H:%M:%S')] [post-fs-data] $1" >> "$LOGFILE"
}

log_msg "MagiskHideX post-fs-data started"
log_msg "Magisk version: $MAGISK_VER_CODE"
log_msg "Zygisk enabled: $ZYGISK_ENABLED"

# Check if DenyList enforcement is active — warn if so
ENFORCE_DENYLIST=$(magisk --denylist status 2>/dev/null)
log_msg "DenyList enforce status: $ENFORCE_DENYLIST"

# Ensure data directory permissions are correct
chmod 700 "$DATADIR"

log_msg "post-fs-data done"
