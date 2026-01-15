# README.md

# SDLite

> **SDLite** is a small, simple 2D game engine written in **C**, built on top of **SDL2**.
>
> It is the evolution of my earlier project **Pebble**, rebuilt with a cleaner structure and continued active development.

---

## ✨ Goals

SDLite focuses on being:

- **Small on purpose** – Only reusable building blocks, no bloated systems
- **C for some reason** – Readable C that kinda works (just kidding).
- **Main-driven** – Your game logic stays in `main.c`
- **SDL2-friendly** – SDL is wrapped just enough to stay pleasant
- **Hackable** – Easy to read, easy to modify, easy to extend

---

## 🧠 Philosophy

SDLite is built around a few simple principles:

- **Clarity over abstraction**  
  You should be able to understand the engine just by reading the source.

- **The engine helps, it does not decide**  
  No rigid architecture. No forced ECS (because im lazy).

- **Small engine, real projects**  
  SDLite provides reusable tools — your game remains fully yours.

---

## 🧩 Features

- 🪟 Window & renderer wrapper  
- 🖼️ Texture handling (loading, scaling, drawing)  
- 🎮 Input helpers (pressed / held detection)  
- ⏱️ Delta-time helper  
- ➗ Vec2 math utilities  
- 📦 Simple physics helpers  
  - Rectangle overlap detection  
  - Clamping  
- 🎥 Camera (follow + clamp)  
- 👾 Entities  
  - Position  
  - Sprite  
  - Basic animations (frame cycling)  
- 🧱 Tile / platform draw helpers

---

## 📦 Project Structure

```text
include/engine/   → Public engine headers
src/engine/       → Engine implementation
src/main.c        → Your game logic lives here
res/              → Assets (you have to copy them)
bin/debug/        → Debug builds
bin/release/      → Release builds
scripts/          → Build scripts
```

---

## 🚀 Getting Started

SDLite requires **SDL2** and **SDL2_image** to be installed on your system.

👉 Full installation and build instructions have been moved to:  
**INSTALL.md**

---

## 📜 License

SDLite is released under the **MIT License**.

You are free to use it in personal or commercial projects, modify it, and redistribute it with attribution.

---

## 💬 Notes

This is a personal project built for learning and experimentation, but it is designed to be usable by others.  
If you find it useful, feel free to explore, fork, or build on top of it.

---
