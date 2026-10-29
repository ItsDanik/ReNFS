#!/bin/sh
# The macOS release: ReNFS.app, one program for Apple silicon and Intel (macOS
# 10.13 and later), SDL2 inside the bundle, with a README and the licences, as
#   dist/renfs-<version>-macos.zip
# usage: tools/release_macos.sh        (on a Mac: needs Xcode's command line tools,
#                                       cmake, pkg-config and curl)
# SDL2's source is fetched and built here for both processors, as a shared library
# that goes into ReNFS.app/Contents/Frameworks.  The bundle is signed ad hoc (no
# developer identity), so a player opens it the first time through System Settings
# (the README says how).
set -e
cd "$(dirname "$0")/.."
ROOT=$PWD
V=$(sh tools/version.sh)
NAME=renfs-$V-macos
SDL=SDL2-2.32.10
SDL_SHA256=5f5993c530f084535c65a6879e9b26ad441169b3e25d789d83287040a9ca5165
W=build/release-macos
ARCHS="arm64;x86_64"
export MACOSX_DEPLOYMENT_TARGET=10.13
APP=dist/$NAME/ReNFS.app

rm -rf $W/game "dist/$NAME"
mkdir -p $W/dl $W/game "dist/$NAME"
# SDL's source: fetched under another name and kept only when whole and its sum right,
# unpacked aside and moved in, so an interrupted run leaves nothing a later one trusts
if [ ! -f $W/dl/$SDL.tar.gz ]; then
    curl -fsSL -o $W/dl/$SDL.tar.gz.part https://github.com/libsdl-org/SDL/releases/download/release-2.32.10/$SDL.tar.gz
    mv $W/dl/$SDL.tar.gz.part $W/dl/$SDL.tar.gz
fi
if [ "$(shasum -a 256 $W/dl/$SDL.tar.gz | cut -d' ' -f1)" != "$SDL_SHA256" ]; then
    rm -f $W/dl/$SDL.tar.gz
    echo "$SDL.tar.gz: wrong sum (removed; run again)" >&2
    exit 1
fi
if [ ! -d $W/$SDL ]; then
    rm -rf $W/unpack && mkdir -p $W/unpack
    tar -xzf $W/dl/$SDL.tar.gz -C $W/unpack
    mv $W/unpack/$SDL $W/$SDL && rmdir $W/unpack
fi
# SDL built for both processors and the oldest macOS; built again when any of them changed
STAMP="$SDL $ARCHS $MACOSX_DEPLOYMENT_TARGET"
if [ ! -f $W/prefix/lib/pkgconfig/sdl2.pc ] || [ "$(cat $W/prefix/built 2> /dev/null)" != "$STAMP" ]; then
    rm -rf $W/sdl $W/prefix
    { cmake -S $W/$SDL -B $W/sdl -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=$ROOT/$W/prefix \
          -DCMAKE_OSX_ARCHITECTURES="$ARCHS" -DSDL_SHARED=ON -DSDL_STATIC=OFF -DSDL_TEST=OFF &&
      cmake --build $W/sdl -j"$(sysctl -n hw.ncpu)" &&
      cmake --install $W/sdl; } > $W/sdl.log 2>&1 || { tail -20 $W/sdl.log >&2; exit 1; }
    echo "$STAMP" > $W/prefix/built
fi
PKG_CONFIG_PATH=$ROOT/$W/prefix/lib/pkgconfig PKG_CONFIG_LIBDIR= cmake -S port -B $W/game \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="$ARCHS" > $W/cmake.log
cmake --build $W/game -j"$(sysctl -n hw.ncpu)" > $W/build.log 2>&1 ||
    { grep -E ' error' $W/build.log >&2 || tail -20 $W/build.log >&2; exit 1; }
grep -E 'warning: implicit' $W/build.log || true

# the bundle: the program, SDL2 beside it in Frameworks, found through the rpath
mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Frameworks" "$APP/Contents/Resources"
cp $W/game/renfs "$APP/Contents/MacOS/renfs"
# (otool lists the libraries once for each processor: every name SDL has there made @rpath's)
LIBS=$(otool -L "$APP/Contents/MacOS/renfs" | awk '/libSDL2/ { print $1 }' | sort -u)
DYLIB=$(for l in $LIBS; do basename "$l"; done | sort -u)
[ -n "$DYLIB" ] && [ "$(echo "$DYLIB" | wc -l)" -eq 1 ] || { echo "SDL2 linked as: $LIBS" >&2; exit 1; }
cp "$W/prefix/lib/$DYLIB" "$APP/Contents/Frameworks/"
install_name_tool -id "@rpath/$DYLIB" "$APP/Contents/Frameworks/$DYLIB"
for l in $LIBS; do
    [ "$l" = "@rpath/$DYLIB" ] || install_name_tool -change "$l" "@rpath/$DYLIB" "$APP/Contents/MacOS/renfs"
done
install_name_tool -add_rpath @executable_path/../Frameworks "$APP/Contents/MacOS/renfs"
for a in x86_64 arm64; do
    if otool -arch $a -L "$APP/Contents/MacOS/renfs" | awk '/libSDL2/ { print $1 }' | grep -qvx "@rpath/$DYLIB"; then
        echo "$a: SDL2 not found through the rpath" >&2; exit 1
    fi
    lipo "$APP/Contents/Frameworks/$DYLIB" -verify_arch $a || { echo "$DYLIB has no $a" >&2; exit 1; }
done
strip -x "$APP/Contents/MacOS/renfs"
cat > "$APP/Contents/Info.plist" <<EOT
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>CFBundleName</key>               <string>ReNFS</string>
  <key>CFBundleDisplayName</key>        <string>ReNFS</string>
  <key>CFBundleIdentifier</key>         <string>io.github.itsdanik.renfs</string>
  <key>CFBundleExecutable</key>         <string>renfs</string>
  <key>CFBundlePackageType</key>        <string>APPL</string>
  <key>CFBundleVersion</key>            <string>$V</string>
  <key>CFBundleShortVersionString</key> <string>$V</string>
  <key>CFBundleInfoDictionaryVersion</key> <string>6.0</string>
  <key>LSMinimumSystemVersion</key>     <string>$MACOSX_DEPLOYMENT_TARGET</string>
  <key>LSApplicationCategoryType</key>  <string>public.app-category.racing-games</string>
  <key>NSHighResolutionCapable</key>    <true/>
  <key>NSHumanReadableCopyright</key>   <string>GPL-3.0. The Need for Speed is a trademark of Electronic Arts.</string>
</dict>
</plist>
EOT
codesign --force --sign - "$APP/Contents/Frameworks/$DYLIB" 2> /dev/null
codesign --force --sign - "$APP" 2> /dev/null

cp LICENSE "dist/$NAME/LICENSE.txt"
cp "$W/$SDL/LICENSE.txt" "dist/$NAME/LICENSE-SDL2.txt"
sed "s/@VERSION@/$V/" tools/release_macos_readme.txt > "dist/$NAME/README.txt"
rm -f "dist/$NAME.zip"
(cd dist && ditto -c -k --keepParent "$NAME" "$NAME.zip")
echo "what it needs:"
lipo -archs "$APP/Contents/MacOS/renfs"
echo "  $DYLIB: $(lipo -archs "$APP/Contents/Frameworks/$DYLIB")"
otool -arch arm64 -L "$APP/Contents/MacOS/renfs" | tail -n +2
for f in "MacOS/renfs" "Frameworks/$DYLIB"; do
    for a in x86_64 arm64; do
        echo "  $f $a from macOS $(otool -arch $a -l "$APP/Contents/$f" | awk '/LC_BUILD_VERSION|LC_VERSION_MIN/ { f = 1 } f && /minos|version/ { print $2; exit }')"
    done
done
codesign --verify --deep --strict "$APP" && echo "signed (ad hoc)"
ls -la "dist/$NAME.zip"
