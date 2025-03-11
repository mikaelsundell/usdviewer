#!/bin/bash
##  Copyright 2022-present Contributors to the jobman project.
##  SPDX-License-Identifier: BSD-3-Clause
##  https://github.com/mikaelsundell/jobman

mkdir AppIcon.iconset
sips -z 16 16     AppIcon@2x.png --out AppIcon.iconset/icon_16x16.png
sips -z 32 32     AppIcon@2x.png --out AppIcon.iconset/icon_16x16@2x.png
sips -z 32 32     AppIcon@2x.png --out AppIcon.iconset/icon_32x32.png
sips -z 64 64     AppIcon@2x.png --out AppIcon.iconset/icon_32x32@2x.png
sips -z 128 128   AppIcon@2x.png --out AppIcon.iconset/icon_128x128.png
sips -z 256 256   AppIcon@2x.png --out AppIcon.iconset/icon_128x128@2x.png
sips -z 256 256   AppIcon@2x.png --out AppIcon.iconset/icon_256x256.png
sips -z 512 512   AppIcon@2x.png --out AppIcon.iconset/icon_256x256@2x.png
sips -z 512 512   AppIcon@2x.png --out AppIcon.iconset/icon_512x512.png
cp AppIcon@2x.png        AppIcon.iconset/icon_512x512@2x.png
iconutil -c icns AppIcon.iconset