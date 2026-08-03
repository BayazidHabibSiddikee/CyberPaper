#!/bin/bash
# install-cpp.sh — Build and install C++ sworddeck + swordfm
set -e

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PREFIX="${PREFIX:-/usr/local}"

echo "=== Building Sworddeck (C++) ==="
cmake -B "$HERE/cpp-sworddeck/build" -S "$HERE/cpp-sworddeck" -DCMAKE_BUILD_TYPE=Release
cmake --build "$HERE/cpp-sworddeck/build" -j"$(nproc)"
echo "  -> cpp-sworddeck/build/sworddeck OK"

echo ""
echo "=== Building SwordFM (C++) ==="
cmake -B "$HERE/cpp-filemanager/build" -S "$HERE/cpp-filemanager" -DCMAKE_BUILD_TYPE=Release
cmake --build "$HERE/cpp-filemanager/build" -j"$(nproc)"
echo "  -> cpp-filemanager/build/swordfm OK"

echo ""
echo "=== Installing to $PREFIX/bin ==="
install -Dm755 "$HERE/cpp-sworddeck/build/sworddeck"  "$PREFIX/bin/sworddeck"
install -Dm755 "$HERE/cpp-filemanager/build/swordfm"  "$PREFIX/bin/swordfm"

# Helper scripts SwordFM shells out to.
install -Dm755 "$HERE/swordconv"  "$PREFIX/bin/swordconv"
install -Dm755 "$HERE/swordgraph" "$PREFIX/bin/swordgraph"
install -Dm755 "$HERE/swordshare" "$PREFIX/bin/swordshare"

# Desktop entry for swordfm.
#
# Exec must be an absolute path: desktop launchers run with a minimal PATH
# (typically just /usr/local/bin:/usr/bin), not the shell's, so a bare
# "Exec=swordfm" either fails to resolve or picks up a stale copy elsewhere
# on disk.
mkdir -p "$HOME/.local/share/applications"
cat > "$HOME/.local/share/applications/swordfm.desktop" << EOF
[Desktop Entry]
Type=Application
Version=1.0
Name=SwordFM
GenericName=File Manager
Comment=Qt6 file manager with preview, recursive search and folder graphs
Exec=$PREFIX/bin/swordfm %f
TryExec=$PREFIX/bin/swordfm
Icon=system-file-manager
Terminal=false
StartupNotify=true
StartupWMClass=swordfm
Categories=System;FileTools;
MimeType=inode/directory;
Keywords=file;manager;browser;explorer;folder;
EOF

update-desktop-database "$HOME/.local/share/applications" 2>/dev/null || true

echo "  -> Installed sworddeck + swordfm to $PREFIX/bin/"
echo "  -> Desktop entry created at ~/.local/share/applications/swordfm.desktop"
echo ""
echo "Done! Run: sworddeck  or  swordfm"
