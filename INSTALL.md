# INSTALL.md

# Installing & Building SDLite

This document explains how to install dependencies and build SDLite on supported platforms.

---

## 📦 Dependencies

SDLite requires:

- SDL2
- SDL2_image
- GCC or Clang

---

## 🪟 Windows

### Recommended Compiler

It is recommended to use the **latest GCC (POSIX-SEH, UCRT64)** build from:

> https://winlibs.com/#download-release

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

SDLite includes a custom Windows installer that automatically sets everything up for you.
No Git, no Python, no PowerShell, and no external tools required.

Just run the installer and you get a ready-to-build project.

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

- Download all required components using the native Windows networking API (WinHTTP):
  - SDLite source (GitHub ZIP archive)
  - SDL2 MinGW development package
  - SDL2_image MinGW development package

- Extract ZIP files using Windows' built‑in ZIP support
  (no PowerShell, no 7‑Zip, no external tools)

- Automatically fixes common ZIP layout issues, including:
  - GitHub double-folder nesting (e.g. `Repo-main/Repo-main/...`)
  - SDL MinGW layouts such as:
    - `SDL2-2.xx.x/x86_64-w64-mingw32/...`
    - Extra wrapper folders

- Ensures that important files remain files (not folders), such as:
  - `README.md`
  - `INSTALL.md`
  - `.sublime-project`

- Cleans up after itself (no leftover `.downloads` or temp folders)

---

### How to use it

1. Download `SDLiteSetup.exe` from this repos release page
2. Run it
3. Choose where you want SDLite installed
4. Wait for downloads and extraction to finish

That’s it. The folder will be fully ready for building.

---

### Requirements

- Windows 10 or Windows 11
- Internet connection

Nothing else required. The installer is compiled statically.

---

### If something looks wrong

At the end of installation, the installer performs basic validation and prints warnings (not crashes) if something looks missing, such as:

- SDL2 headers not detected
- SDL2_image headers not detected

If you see warnings, the most common cause is that the `external/SDL2` folders have subfolders called `x86_64-w64-mingw32` and `i686-w64-mingw32` (32-bit).
Just move these folders to external and rename them to SDL2 and same for SDL2_image.

---

### Technical details (for curious users)

The installer is written in pure C (C17) using the Win32 API. It uses:

- WinHTTP for downloads
- Shell.Application COM for ZIP extraction

---

### Building (Windows)

You can build using the included Sublime project or manually with the provided commands.

**Debug build:**

```bash
gcc -std=c17 -g -O0 -DDEBUG -Wall -Wextra \
  -I include\\engine \
  -I external\\SDL2\\include\\SDL2 \
  -I external\\SDL2_image\\include\\SDL2 \
  src\\*.c src\\engine\\*.c \
  -L external\\SDL2\\lib \
  -L external\\SDL2_image\\lib \
  -lSDL2 -lSDL2_image -lopengl32 \
  -o bin\\debug\\main_debug.exe
```

**Release build:**

```bash
gcc -std=c17 -O2 -DNDEBUG -Wall -Wextra \
  -I include\\engine \
  -I external\\SDL2\\include\\SDL2 \
  src\\*.c src\\engine\\*.c \
  -L external\\SDL2\\lib \
  -L external\\SDL2_image\\lib \
  -lSDL2 -lSDL2_image -lopengl32 \
  -o bin\\release\\main_release.exe
```

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

## 🔁 Cross-compiling for Windows (from Linux)

If you want to build Windows binaries on Linux, install MinGW:

### Arch Linux

```bash
sudo pacman -S mingw-w64-gcc
```

### Debian / Ubuntu

```bash
sudo apt install mingw-w64
```

Then use the provided script with:

```bash
./scripts/compile.sh --build win-debug
./scripts/compile.sh --build win-release
```

---

## 🛠️ Build Script Usage

A helper script is included for Linux builds:

```bash
chmod +x scripts/compile.sh

./scripts/compile.sh --build linux-debug
./scripts/compile.sh --build linux-release
./scripts/compile.sh --build win-debug
./scripts/compile.sh --build win-release

# Optional: run after building
./scripts/compile.sh --build linux-release --run
```

---

## 📁 Assets

After building, make sure the `res` folder is copied into:

```text
bin/debug/
bin/release/
```

Otherwise the executable will not find your assets.

---

## ✅ That's it

If everything is installed correctly, you should be able to build and run SDLite without additional setup.

