# INSTALL.md

# Installing

This document explains how to install dependencies and build SDLite on supported platforms.

---

## 📦 Dependencies

SDLite requires:

- SDL2
- SDL2_image
- GCC or Clang

---

## 🪟 Windows (Manually)

### Recommended Compiler

Recommended: [MinGW-w64 GCC](https://winlibs.com/#download-release)

Make sure to choose the latest GCC (with POSIX threads) + MinGW-w64 x.x.x (UCRT)

Extract the archive to:

```text
C:\mingw64\bin
```

Then add this to your system **PATH** so the compiler is available in the terminal.

Next, download the **SDL2** and **SDL2_image** MinGW-w64 development packages from the official SDL GitHub releases:

- SDL2: https://github.com/libsdl-org/SDL/releases
- SDL2_image: https://github.com/libsdl-org/SDL_image/releases

Extract them into a folder such as:

```text
external/SDL2/
external/SDL2_image/
```

so the include and lib paths used in the build commands match your layout.

---

## Windows Installer (SDLiteSetup.exe)

SDLite includes a custom Windows installer written in python that automatically
handles all of the setup for you except for mingw, it builds out the folder structure
and fetches SDL dependencies for you.

---

### What the installer does

When you run `SDLiteSetup.exe`, it will:

- Ask you to choose an install location
- Create this folder structure:

```text
SDLite/
  include/
  src/
  res/
  external/
    SDL2/
    SDL2_image/
  bin/
    debug/
    release/
```

---

### How to use it

1. Download `SDLiteSetup.exe` from this repos release page
2. Run it
3. Choose where you want SDLite installed
4. Customize repo paths, downloads, structure or leave default.
5. Wait for downloads and extraction to finish

That’s it. The folder will be fully ready for building.

---

### Requirements

- Windows 10 or Windows 11
- Internet connection

Nothing else required. The installer is bundled with python.

---

## 🐧 Linux

Install dependencies using your package manager.

### Arch Linux

```bash
sudo pacman -S sdl2 sdl2_image gcc
```

### Debian / Ubuntu

```bash
sudo apt install gcc libsdl2-dev libsdl2-image-dev
```

---

