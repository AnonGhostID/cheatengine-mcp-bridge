#!/usr/bin/env bash
# build.sh — Cross-compile ce_mcp_bridge_plugin for Windows (x64 + x86)
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LUA_SRC="$SCRIPT_DIR/../MCP_Server/ce_mcp_bridge.lua"
PAYLOAD_H="$SCRIPT_DIR/lua_payload.h"
PLUGIN_C="$SCRIPT_DIR/plugin.c"
DIST_DIR="$SCRIPT_DIR/dist"

mkdir -p "$DIST_DIR"

echo "=== Step 1: Generating Lua payload header ==="
python3 "$SCRIPT_DIR/generate_payload.py" "$LUA_SRC" "$PAYLOAD_H"

# Common compiler flags:
#  -shared          — build a DLL
#  -O2              — optimise (reduces DLL size with big string constant)
#  -Wall            — warnings
#  -Wl,--kill-at    — strip @N from stdcall symbol names (CE expects undecorated exports)
#  -static-libgcc   — embed libgcc so CE doesn't need it on the Windows host
COMMON_FLAGS="-shared -O2 -Wall -static-libgcc \
  -Wl,--kill-at \
  -I\"$SCRIPT_DIR\""

echo ""
echo "=== Step 2: Building 64-bit DLL ==="
x86_64-w64-mingw32-gcc $COMMON_FLAGS \
  -D_AMD64_ \
  "$PLUGIN_C" \
  -o "$DIST_DIR/ce_mcp_bridge_plugin_x64.dll"
echo "  -> $DIST_DIR/ce_mcp_bridge_plugin_x64.dll"

echo ""
echo "=== Build complete ==="
ls -lh "$DIST_DIR"/*.dll

echo ""
echo "Verifying exports (requires binutils-mingw-w64):"
if command -v x86_64-w64-mingw32-nm &>/dev/null; then
    echo "--- x64 exports ---"
    x86_64-w64-mingw32-nm --defined-only -g "$DIST_DIR/ce_mcp_bridge_plugin_x64.dll" \
      | grep -i "ceplugin" || true
fi
