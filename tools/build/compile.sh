#!/usr/bin/env bash
set -e

# =========================
# SDLite Linux build script
# =========================

CC=gcc
CFLAGS_COMMON="-std=c17 -Wall -Wextra"
SRC="src/*.c src/engine/*.c"
INCLUDES="-Iinclude"
OUT_DEBUG="bin/debug/main_debug"
OUT_RELEASE="bin/release/main_release"

# SDL flags from system
SDL_CFLAGS="$(sdl2-config --cflags)"
SDL_LIBS="$(sdl2-config --libs) -lSDL2_image"

mkdir -p bin/debug bin/release

case "$1" in
  debug)
    echo "[SDLite] Building DEBUG..."
    $CC $CFLAGS_COMMON -g -O0 -DDEBUG \
      $INCLUDES $SDL_CFLAGS \
      $SRC \
      -o "$OUT_DEBUG" \
      $SDL_LIBS
    echo "Built: $OUT_DEBUG"
    ;;

  release)
    echo "[SDLite] Building RELEASE..."
    $CC $CFLAGS_COMMON -O2 -DNDEBUG \
      $INCLUDES $SDL_CFLAGS \
      $SRC \
      -o "$OUT_RELEASE" \
      $SDL_LIBS
    echo "Built: $OUT_RELEASE"
    ;;

  *)
    echo "Usage:"
    echo "  ./compile.sh debug"
    echo "  ./compile.sh release"
    exit 1
    ;;
esac
