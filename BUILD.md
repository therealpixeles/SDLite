# Building SDLite, Installer, and others.

This document explains how to build projects that use **SDLite** (and how to build SDLite itself) on Windows and Linux.

> If you just want the easiest Windows setup, use the **SDLite Installer** release EXE. It downloads SDLite + SDL2 + SDL2_image and lays out the correct folder tree automatically.

---

## Project layout

SDLite expects this structure (created by the installer):

```text
SDLite/
  include/
  src/
  res/
  external/
    SDL2/
      x86_64-w64-mingw32/
        include/
        lib/
        bin/
    SDL2_image/
      x86_64-w64-mingw32/
        include/
        lib/
        bin/
  bin/
    debug/
    release/
```

- `include/` and `src/` are SDLite’s headers and source.
- `res/` is optional assets.
- `external/` contains the SDL2 toolchain folders.
- `bin/` is a convenient output location.

---

## Windows 

Recommended: [MinGW-w64 GCC](https://winlibs.com/#download-release)

Make sure to choose the latest GCC (with POSIX threads) + MinGW-w64 x.x.x (UCRT)

### Building (Windows)

If you build manually (example):

You can build using the included Sublime project or manually with the provided commands.

**Debug build:**

```bash
gcc -std=c17 -g -O0 -DDEBUG -Wall -Wextra \
  -I include \
  -I external\SDL2\x86_64-w64-mingw32\include \
  -I external\SDL2_image\x86_64-w64-mingw32\include \
  src\*.c src\engine\*.c \
  -L external\SDL2\x86_64-w64-mingw32\lib \
  -L external\SDL2_image\x86_64-w64-mingw32\lib \
  -lSDL2 -lSDL2_image \
  -o bin\debug\main_debug.exe
```

**Release build:**

```bash
gcc -std=c17 -O2 -DNDEBUG -Wall -Wextra \
  -I include \
  -I external\SDL2\x86_64-w64-mingw32\include \
  -I external\SDL2_image\x86_64-w64-mingw32\include \
  src\*.c src\engine\*.c \
  -L external\SDL2\x86_64-w64-mingw32\lib \
  -L external\SDL2_image\x86_64-w64-mingw32\lib \
  -lSDL2 -lSDL2_image \
  -o bin\release\main_release.exe
```

---

### 4) Running your program

SDL2 DLLs must be discoverable at runtime. The easiest approach:

- Copy the required DLLs from:
  - `external/SDL2/x86_64-w64-mingw32/bin/`
  - `external/SDL2_image/x86_64-w64-mingw32/bin/`
- Into the same folder as your built `.exe` (for example `bin/debug/`).

Common DLLs you may need next to your exe include:
- `SDL2.dll`
- `SDL2_image.dll`
- and any image format DLLs shipped with SDL2_image (depending on the build)

---

## Linux Build

If you have a Makefile:

```bash
make
```

Manual build example:

```bash
gcc -Iinclude -Iinclude/engine src/*.c src/engine/*.c -o bin/debug/app \
  $(sdl2-config --cflags --libs) -lSDL2_image
```

---

## Building without the installer (Windows manual dependency setup)

If you don’t use the installer:

1. Download the **SDL2 MinGW dev ZIP** and **SDL2_image MinGW dev ZIP**.
2. Extract them.
3. Place them like this:

```text
external/SDL2/x86_64-w64-mingw32/(include,lib,bin)
external/SDL2_image/x86_64-w64-mingw32/(include,lib,bin)
```

If your extracted ZIP already contains the `x86_64-w64-mingw32` folder, put that folder directly under `external/SDL2/` (same for SDL2_image).

---

## Building the installer (Windows)

The installer is written in Python and can be bundled into a single EXE.

### 1) Install Python

Use Python 3.14.x (or a compatible version).

### 2) Install PyInstaller

```bat
py -3.14 -m pip install --upgrade pyinstaller
```

### 3) Build the EXE

From the folder containing `installer.py`:

```bat
py -3.14 -m PyInstaller --onefile --noconsole --name SDLiteSetup installer.py
```

Output:

```text
dist\SDLiteSetup.exe
```

---

## Troubleshooting

### “SDL2.dll was not found” / app won’t start

Copy SDL2 DLLs next to your exe:
- `external/SDL2/x86_64-w64-mingw32/bin/*.dll`
- `external/SDL2_image/x86_64-w64-mingw32/bin/*.dll`

### Include not found / link errors

Double-check your include/library paths:
- `.../include`
- `.../lib`

### 32-bit vs 64-bit mismatch

Make sure you’re using:
- MinGW **x86_64** tools
- SDL2/SDL2_image **x86_64** dev zips

---

## Getting help

If something breaks:
- open an issue
- include your OS, compiler, and the full build command/output
- if using the installer, paste the installer log output

