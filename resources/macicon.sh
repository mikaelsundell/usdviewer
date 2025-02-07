#!/bin/bash

if [ "$#" -ne 1 ]; then
    echo "Usage: $0 image.png"
    exit 1
fi

INPUT_IMAGE="$1"
BASENAME=$(basename "$INPUT_IMAGE" .png)
ICONSET="${BASENAME}.iconset"
ICNS_FILE="${BASENAME}.icns"

if [ ! -f "$INPUT_IMAGE" ]; then
    echo "Error: File '$INPUT_IMAGE' not found!"
    exit 1
fi

mkdir -p "$ICONSET"
sips -z 16 16     "$INPUT_IMAGE" --out "$ICONSET/icon_16x16.png"
sips -z 32 32     "$INPUT_IMAGE" --out "$ICONSET/icon_16x16@2x.png"
sips -z 32 32     "$INPUT_IMAGE" --out "$ICONSET/icon_32x32.png"
sips -z 64 64     "$INPUT_IMAGE" --out "$ICONSET/icon_32x32@2x.png"
sips -z 128 128   "$INPUT_IMAGE" --out "$ICONSET/icon_128x128.png"
sips -z 256 256   "$INPUT_IMAGE" --out "$ICONSET/icon_128x128@2x.png"
sips -z 256 256   "$INPUT_IMAGE" --out "$ICONSET/icon_256x256.png"
sips -z 512 512   "$INPUT_IMAGE" --out "$ICONSET/icon_256x256@2x.png"
sips -z 512 512   "$INPUT_IMAGE" --out "$ICONSET/icon_512x512.png"
cp "$INPUT_IMAGE" "$ICONSET/icon_512x512@2x.png"

iconutil -c icns "$ICONSET" -o "$ICNS_FILE"
rm -rf "$ICONSET"

echo "Created $ICNS_FILE"
