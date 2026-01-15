# 📦 INSTALL.md

> How to install SDLite and its dependencies

This document explains how to set up **SDLite**, **SDL2**, and **SDL2_image** on supported platforms.

---

## 📚 Requirements

SDLite requires:

- SDL2
- SDL2_image
- GCC or Clang

---

## 🪟 Windows (Recommended: Installer)

### ✅ Easiest Method: SDLiteSetup.exe

SDLite includes a custom Windows installer that:

- Creates the correct folder structure
- Downloads SDL2 + SDL2_image
- Places everything where SDLite expects it

Steps:

1. Download `SDLiteSetup.exe` from the Releases page
2. Run it
3. Choose an install folder
4. Wait for setup to complete

After that, the folder is immediately ready to build.

---

## 🪟 Windows (Manual Setup)

If you prefer full manual control:

### 1) Install MinGW-w64

Recommended build settings:

- Architecture: x86_64
- Threads: POSIX
- Runtime: UCRT

Download: https://winlibs.com/

Extract somewhere like:

```text
C:\mingw64\
```

Add to PATH:

```text
C:\mingw64\bin
```

Verify:

```bat
gcc --version
```

---

### 2) Download SDL2 and SDL2_image

Official sources:

- SDL2: https://github.com/libsdl-org/SDL/releases
- SDL2_image: https://github.com/libsdl-org/SDL_image/releases

Download the **MinGW-w64 development packages** (not source-only).

---

### 3) Place files in this layout

Your extracted folders should end up like:

```text
external/SDL2/x86_64-w64-mingw32/(include, lib, bin)
external/SDL2_image/x86_64-w64-mingw32/(include, lib, bin)
```

If the zip already contains `x86_64-w64-mingw32/`, place that folder directly inside `external/SDL2/`.

---

## 🐧 Linux

Install dependencies using your system package manager.

### Arch Linux

```bash
sudo pacman -S sdl2 sdl2_image gcc
```

### Debian / Ubuntu

```bash
sudo apt install gcc libsdl2-dev libsdl2-image-dev
```

---

## 🧰 Building the Installer (for developers)

The SDLite installer is written in Python and bundled with PyInstaller.

### Requirements

- Python 3.14+
- PyInstaller

Install PyInstaller:

```bat
py -m pip install --upgrade pyinstaller
```

Build the EXE:

```bat
py -m PyInstaller --onefile --noconsole --name SDLiteSetup installer.py
```

Output file:

```text
dist/SDLiteSetup.exe
```

---

## 🧯 Common Issues

### ❌ gcc not recognized
Your MinGW `bin` folder is not in PATH.

### ❌ SDL headers not found
Check that your folders look like:

```text
external/SDL2/x86_64-w64-mingw32/include/SDL2/SDL.h
```

### ❌ Wrong architecture
Make sure everything is **x86_64**:

- Compiler
- SDL2 package
- SDL2_image package

---

## 🔗 Useful Resources

- SDL Wiki: https://wiki.libsdl.org/
- SDL2 Tutorials: https://lazyfoo.net/tutorials/SDL/
- SDL2 Source: https://github.com/libsdl-org/SDL
- SDL2_image Source: https://github.com/libsdl-org/SDL_image
- PyInstaller Docs: https://pyinstaller.org/

