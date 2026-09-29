#!/bin/bash
set -e

# Check if dkms is installed
if ! command -v dkms &> /dev/null; then
    echo "Error: 'dkms' command not found."
    echo "Please install it first (e.g., sudo pacman -S dkms linux-headers)"
    exit 1
fi

MODULE_NAME="RKKDR"
MODULE_VERSION="0.0.3"
DKMS_SRC_DIR="/usr/src/${MODULE_NAME}-${MODULE_VERSION}"

echo "Installing ${MODULE_NAME} version ${MODULE_VERSION} to DKMS..."

# Copy files to /usr/src
sudo mkdir -p "${DKMS_SRC_DIR}"
sudo cp -r ./* "${DKMS_SRC_DIR}/"

# Remove existing module if present (ignore errors if not)
sudo dkms remove -m "${MODULE_NAME}" -v "${MODULE_VERSION}" --all 2>/dev/null || true

# Find the latest installed kernel version (instead of using uname -r which might be outdated)
KERNEL_VER=$(ls /usr/lib/modules | grep -E '^[0-9]' | sort -V | tail -n 1)
echo "Building against installed kernel: ${KERNEL_VER}"

# Add, build, and install via DKMS for the specific installed kernel
sudo dkms add -m "${MODULE_NAME}" -v "${MODULE_VERSION}"
sudo dkms build -m "${MODULE_NAME}" -v "${MODULE_VERSION}" -k "${KERNEL_VER}"
sudo dkms install -m "${MODULE_NAME}" -v "${MODULE_VERSION}" -k "${KERNEL_VER}"

echo "Configuring module to load automatically on boot..."
echo "${MODULE_NAME}" | sudo tee "/etc/modules-load.d/${MODULE_NAME}.conf" > /dev/null

echo "DKMS installation complete! The module will now rebuild automatically on kernel updates and load on boot."
