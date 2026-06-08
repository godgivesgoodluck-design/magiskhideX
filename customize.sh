#!/system/bin/sh
# MagiskHideX - Installer Script
# Compatible with Magisk Alpha + NeoZygisk

SKIPUNZIP=1

# ─── Color Output ───────────────────────────────────────────────────
ui_print "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
ui_print "     MagiskHideX - Root Hider Module"
ui_print "     Compatible: Magisk Alpha + NeoZygisk"
ui_print "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

# ─── Zygisk Check ───────────────────────────────────────────────────
if [ "$ZYGISK_ENABLED" != "1" ]; then
    ui_print "! Zygisk is NOT enabled."
    ui_print "! Please enable Zygisk in Magisk settings OR install NeoZygisk."
    ui_print "! Aborting installation."
    abort
fi

ui_print "- Zygisk detected: OK"

# ─── API Version Check ──────────────────────────────────────────────
if [ "$MAGISK_VER_CODE" -lt 26402 ]; then
    ui_print "! Magisk version too old (need >= 26402)"
    abort
fi

ui_print "- Magisk version: $MAGISK_VER_CODE OK"

# ─── ABI Detection ──────────────────────────────────────────────────
SUPPORTED_ABIS=""
for abi in $ARCH; do
    case $abi in
        arm64-v8a)   SUPPORTED_ABIS="$SUPPORTED_ABIS arm64-v8a" ;;
        armeabi-v7a) SUPPORTED_ABIS="$SUPPORTED_ABIS armeabi-v7a" ;;
        x86_64)      SUPPORTED_ABIS="$SUPPORTED_ABIS x86_64" ;;
        x86)         SUPPORTED_ABIS="$SUPPORTED_ABIS x86" ;;
    esac
done
ui_print "- Device ABI: $ARCH"

# ─── Extract Module Files ───────────────────────────────────────────
ui_print "- Extracting module files..."
unzip -o "$ZIPFILE" 'module.prop' -d "$MODPATH" >/dev/null 2>&1
unzip -o "$ZIPFILE" 'zygisk/*' -d "$MODPATH" >/dev/null 2>&1
unzip -o "$ZIPFILE" 'service.sh' -d "$MODPATH" >/dev/null 2>&1
unzip -o "$ZIPFILE" 'post-fs-data.sh' -d "$MODPATH" >/dev/null 2>&1
unzip -o "$ZIPFILE" 'action.sh' -d "$MODPATH" >/dev/null 2>&1

# ─── Create Data Directory ──────────────────────────────────────────
DATADIR="/data/adb/magiskhidex"
mkdir -p "$DATADIR"
chmod 700 "$DATADIR"

ui_print "- Data directory: $DATADIR"

# ─── NeoZygisk Compatibility Note ───────────────────────────────────
ui_print ""
ui_print "━━━━ NeoZygisk Compatibility ━━━━"
if [ -d "/data/adb/modules/neozygisk" ] || [ -d "/data/adb/modules/zygisksu" ]; then
    ui_print "- NeoZygisk/ZygiskNext detected!"
    ui_print "  Make sure 'Enforce DenyList' is DISABLED"
    ui_print "  in your ZygiskNext/NeoZygisk settings."
else
    ui_print "- Using Magisk built-in Zygisk."
    ui_print "  Make sure 'Enforce DenyList' is DISABLED"
    ui_print "  in Magisk settings."
fi

# ─── Usage Instructions ─────────────────────────────────────────────
ui_print ""
ui_print "━━━━ How to Use ━━━━"
ui_print "1. Add target apps to Magisk DenyList"
ui_print "2. DISABLE 'Enforce DenyList' in Magisk"
ui_print "3. Reboot device"
ui_print ""
ui_print "Whitelist Mode (optional):"
ui_print "  touch /data/adb/magiskhidex/whitelist"
ui_print "  (Only root-granted apps can get root)"
ui_print ""
ui_print "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
ui_print "  Installation complete! Reboot to apply."
ui_print "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

# ─── Set Permissions ────────────────────────────────────────────────
set_perm_recursive "$MODPATH" root root 0755 0644
set_perm "$MODPATH/service.sh" root root 0755
set_perm "$MODPATH/post-fs-data.sh" root root 0755
if [ -f "$MODPATH/action.sh" ]; then
    set_perm "$MODPATH/action.sh" root root 0755
fi
