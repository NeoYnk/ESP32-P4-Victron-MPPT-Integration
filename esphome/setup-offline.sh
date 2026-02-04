#!/bin/bash
# Setup script for offline ESPHome Victron VE.Direct integration
# Run this once while you have internet access to download all required components

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
COMPONENTS_DIR="${SCRIPT_DIR}/components"

echo "=== ESPHome Victron VE.Direct Offline Setup ==="
echo ""

# Create components directory
mkdir -p "${COMPONENTS_DIR}"

# Download m3_vedirect component from GitHub
echo "[1/2] Downloading m3_vedirect component..."
if [ -d "${COMPONENTS_DIR}/m3_vedirect" ]; then
    echo "  -> m3_vedirect already exists, updating..."
    rm -rf "${COMPONENTS_DIR}/m3_vedirect"
fi

# Clone only the components folder using sparse checkout
TEMP_DIR=$(mktemp -d)
cd "${TEMP_DIR}"
git clone --depth 1 --filter=blob:none --sparse \
    https://github.com/krahabb/esphome-victron-vedirect.git repo
cd repo
git sparse-checkout set components/m3_vedirect
cp -r components/m3_vedirect "${COMPONENTS_DIR}/"
cd "${SCRIPT_DIR}"
rm -rf "${TEMP_DIR}"
echo "  -> m3_vedirect downloaded successfully"

# Verify victron_charge_limit custom component exists
echo "[2/2] Checking victron_charge_limit component..."
if [ -d "${SCRIPT_DIR}/custom_components/victron_charge_limit" ]; then
    echo "  -> victron_charge_limit already exists"
else
    echo "  -> ERROR: victron_charge_limit not found in custom_components/"
    echo "     Please ensure the custom_components directory is present"
    exit 1
fi

echo ""
echo "=== Setup Complete ==="
echo ""
echo "Components installed to: ${COMPONENTS_DIR}"
echo ""
echo "Directory structure:"
find "${COMPONENTS_DIR}" -type f -name "*.py" -o -name "*.h" -o -name "*.cpp" | head -20
echo ""
echo "You can now use the offline configuration:"
echo "  esphome run esp32-p4-victron-mppt-offline.yaml"
echo ""
