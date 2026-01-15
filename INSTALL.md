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

