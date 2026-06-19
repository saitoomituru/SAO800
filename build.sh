#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

cmake .. \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_OSX_ARCHITECTURES=x86_64

cmake --build . --parallel "$(sysctl -n hw.logicalcpu)"

echo ""
echo "=== ビルド完了 ==="
echo "プラグインをインストール:"
echo "  cmake --install . "
echo "その後OBSを再起動してください"
