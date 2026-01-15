# 🛠️ BUILD.md

> Build instructions for SDLite and projects that use SDLite

Clean, consistent build guidance for **Windows**, **Linux**, and optional cross‑compiling.

---

## 📁 Expected Project Layout

SDLite expects this structure (created by the installer or manual setup):

```text
SDLite/
  include/
  src/
  res/                (optional assets)
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

- `include/` + `src/` → SDLite source code
- `res/` → assets (textures, audio, fonts, etc.)
- `external/` → SDL2 + SDL2_image dependencies
- `bin/` → compiled executables

---

## 🪟 Windows (MinGW-w64)

### ✅ Recommended compiler
Use **MinGW-w64 GCC** (WinLibs builds work best):

- Architecture: **x86_64**
- Threads: **POSIX**
- Runtime: **UCRT**

Download: https://winlibs.com/

Make sure this works in your terminal:

```bat
gcc --version
```

---

## 🔨 Manual Build (Windows)

> ℹ️ SDL headers normally live in:
> `.../include/SDL2/SDL.h`
> so your include paths should end with `/include/SDL2`

### 🐛 Debug build

```bat
gcc -std=c17 -g -O0 -DDEBUG -Wall -Wextra ^
  -I include ^
  -I external\SDL2\x86_64-w64-mingw32\include\SDL2 ^
  -I external\SDL2_image\x86_64-w64-mingw32\include\SDL2 ^
  src\*.c src\engine\*.c ^
  -L external\SDL2\x86_64-w64-mingw32\lib ^
  -L external\SDL2_image\x86_64-w64-mingw32\lib ^
  -lSDL2main -lSDL2 -lSDL2_image ^
  -o bin\debug\main_debug.exe
```

### 🚀 Release build

```bat
gcc -std=c17 -O2 -DNDEBUG -Wall -Wextra ^
  -I include ^
  -I external\SDL2\x86_64-w64-mingw32\include\SDL2 ^
  -I external\SDL2_image\x86_64-w64-mingw32\include\SDL2 ^
  src\*.c src\engine\*.c ^
  -L external\SDL2\x86_64-w64-mingw32\lib ^
  -L external\SDL2_image\x86_64-w64-mingw32\lib ^
  -lSDL2main -lSDL2 -lSDL2_image ^
  -o bin\release\main_release.exe
```

---

## ▶️ Running on Windows (DLLs required)

Windows requires SDL DLLs next to your executable.

Copy these into:

```text
bin/debug/
bin/release/
```

From:

```text
external/SDL2/x86_64-w64-mingw32/bin/
external/SDL2_image/x86_64-w64-mingw32/bin/
```

Common files:

- `SDL2.dll`
- `SDL2_image.dll`
- Any extra image format DLLs (depending on build)

If missing, Windows will show:
> ❌ "SDL2.dll was not found"

---

## 🐧 Linux Builds

Install dependencies with your package manager:

### Arch Linux

```bash
sudo pacman -S sdl2 sdl2_image gcc
```

### Debian / Ubuntu

```bash
sudo apt install gcc libsdl2-dev libsdl2-image-dev
```

### Example Linux build

```bash
gcc -std=c17 -O2 -Wall -Wextra \
  src/*.c src/engine/*.c \
  -I include \
  $(sdl2-config --cflags --libs) -lSDL2_image \
  -o bin/release/app
```

---

## 🧪 Cross-compiling Windows binaries from Linux

Install MinGW:

### Arch
```bash
sudo pacman -S mingw-w64-gcc
```

### Debian / Ubuntu
```bash
sudo apt install mingw-w64
```

Typical compiler:

```bash
x86_64-w64-mingw32-gcc
```

You can reuse the same Windows flags, just replace `gcc` with the MinGW compiler.

---

## 📦 Assets (`res` folder)

If your project uses assets, copy them next to your executable:

```text
bin/debug/res/
bin/release/res/
```

If missing, your program may run but fail to load textures, fonts, audio, etc.

---

## 🧯 Troubleshooting

### ❌ SDL2.dll not found
You forgot to copy DLLs next to the executable.

### ❌ Cannot find SDL.h
Your include path should end with:

```text
.../include/SDL2
```

Not just:

```text
.../include
```

### ❌ 32-bit vs 64-bit issues
All of these must match:

- Compiler: `x86_64-w64-mingw32-gcc`
- SDL2 dev package: x86_64
- SDL2_image dev package: x86_64

---

## 🔗 Useful Links

- SDL2 Docs: https://wiki.libsdl.org/SDL2/
- SDL2_image Docs: https://wiki.libsdl.org/SDL2_image/
- SDL2 Releases: https://github.com/libsdl-org/SDL/releases
- SDL2_image Releases: https://github.com/libsdl-org/SDL_image/releases
- WinLibs (MinGW-w64): https://winlibs.com/

