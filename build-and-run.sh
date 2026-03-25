#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT_DIR/build}"
CONFIG="${CONFIG:-Release}"

case "$(uname -s)" in
  Darwin)
    PLATFORM="macOS"
    ;;
  Linux)
    PLATFORM="Linux"
    ;;
  *)
    echo "Unsupported platform: $(uname -s)" >&2
    exit 1
    ;;
esac

echo "Configuring pointmod for ${PLATFORM}..."
cmake -S "$ROOT_DIR" -B "$BUILD_DIR"

echo "Building pointmod..."
cmake --build "$BUILD_DIR" --target pointmod --config "$CONFIG"

APP_PATH=""
for candidate in \
  "$BUILD_DIR/pointmod" \
  "$BUILD_DIR/$CONFIG/pointmod"
do
  if [[ -x "$candidate" ]]; then
    APP_PATH="$candidate"
    break
  fi
done

if [[ -z "$APP_PATH" ]]; then
  echo "Could not find built pointmod executable in $BUILD_DIR" >&2
  exit 1
fi

echo "Running $APP_PATH..."
exec "$APP_PATH" "$@"
