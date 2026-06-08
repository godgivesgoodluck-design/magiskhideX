#!/system/bin/sh
# MagiskHideX - action.sh
# Triggered by Magisk Manager "Action" button (Magisk 28+)
# Toggles whitelist mode on/off

DATADIR="/data/adb/magiskhidex"
WHITELIST_FILE="$DATADIR/whitelist"

mkdir -p "$DATADIR"

if [ -f "$WHITELIST_FILE" ]; then
    rm "$WHITELIST_FILE"
    echo "MagiskHideX: Switched to DENYLIST mode"
    echo "(Apps in DenyList will have root hidden)"
else
    touch "$WHITELIST_FILE"
    echo "MagiskHideX: Switched to WHITELIST mode"
    echo "(Only root-granted apps can access root)"
fi

echo ""
echo "No reboot needed — takes effect immediately."
