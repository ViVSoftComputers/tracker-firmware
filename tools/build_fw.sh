#!/bin/bash
# Quick build for Heltec V3 (LocalGpsTrackLogger)
# Run from WSL2 Ubuntu:
#   wsl -d Ubuntu bash -c "cd ~/src/firmware && bash tools/build_fw.sh"

set -e
FW_DIR="${1:-$HOME/src/firmware}"
cd "$FW_DIR"

rm -f /tmp/fw_build.log
~/.local/bin/pio run -e heltec-v3 > /tmp/fw_build.log 2>&1
echo "EXIT:$?" >> /tmp/fw_build.log

# Copy artifacts
mkdir -p /tmp/artifacts
cp .pio/build/heltec-v3/firmware.factory.bin /tmp/artifacts/firmware-heltec-v3.bin 2>/dev/null || true
cp .pio/build/heltec-v3/partition-table.bin /tmp/artifacts/ 2>/dev/null || true
cp .pio/build/heltec-v3/bootloader.bin /tmp/artifacts/ 2>/dev/null || true
echo "Done. Artifacts in /tmp/artifacts/"