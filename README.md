# PixelCanvas

A Krita-inspired raster drawing application built with **C++20** and **Qt 6** for Linux.

## Features (planned)
- Pressure-sensitive stylus input (Wacom, Huion, XP-Pen) via libinput / XInput2
- Layer system with blend modes (Normal, Multiply, Screen, Overlay…)
- Pluggable brush engine (Round, Eraser, Airbrush, Smudge, Texture)
- Non-destructive filters powered by OpenCV (Blur, Brightness/Contrast, Invert, Sharpen)
- Custom `.paint` project format (JSON manifest + zipped PNGs)

## Team
| Developer | Areas |
|-----------|-------|
| Toma | Canvas, viewport, tablet input, brush engine, layer system, undo/redo, filters, file I/O |
| Member 4 | MainWindow shell, ToolbarPanel, LayerPanel, menus, dialogs, shortcuts |

---

## Project structure

```
pixel_canvas/
├── CMakeLists.txt
├── README.md
├── .gitignore
├── resources/
│   └── app.desktop              ← Linux XDG launcher
└── src/
    ├── main.cpp                 ← Entry point
    │
    │   ── Core (Toma) ──────────────────────────────
    ├── CanvasWidget.h/.cpp      ← QOpenGLWidget, viewport, tablet/mouse events
    ├── Stroke.h                 ← StrokeSample + Stroke data structs
    ├── BrushTip.h/.cpp          ← Abstract BrushTip interface + PixelTip stub
    ├── BrushEngine.h/.cpp       ← Stroke → dab pipeline, spacing, smoothing
    ├── Layer.h                  ← Plain layer data (QImage, opacity, blend mode)
    ├── LayerStack.h/.cpp        ← Layer management + recompositeRect()
    ├── UndoStack.h/.cpp         ← Two-stack snapshot undo/redo
    ├── Filter.h                 ← Abstract Filter interface + 4 concrete classes
    ├── FilterImpls.cpp          ← Stub implementations (real OpenCV logic in Ph.10)
    ├── ProjectIO.h/.cpp         ← Save/load .paint, export flat (Ph.12)
    │
    │   ── UI (Member 4) ────────────────────────────
    ├── MainWindow.h/.cpp        ← QMainWindow shell, menus, docks, signal wiring
    ├── ToolbarPanel.h/.cpp      ← Color picker, brush size, brush list
    └── LayerPanel.h/.cpp        ← Layer list, add/remove, opacity slider
```

---

## How to compile and run

### 1. Install system dependencies

> **Note:** The system `cmake` on Ubuntu 22.04 is version 3.22, which is too old.
> Install a newer one via snap first.

```bash
# cmake 3.25+ via snap
sudo apt remove cmake
sudo snap install cmake --classic
echo 'export PATH="/snap/bin:$PATH"' >> ~/.bashrc
source ~/.bashrc

# Qt6, OpenCV, OpenGL, build tools
sudo apt install \
    qt6-base-dev qt6-base-dev-tools \
    libopencv-dev \
    libgl-dev mesa-common-dev \
    build-essential
```

Optional (tablet input & Wayland):
```bash
sudo apt install \
    qt6-wayland \
    libinput-dev libxi-dev libxcb-xinput-dev
```

### 2. Clone and build

```bash
git clone https://github.com/kustigrenka/pixel_canvas.git
cd pixel_canvas

cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
```

### 3. Run

```bash
./build/PixelCanvas
```

**Wayland:**
```bash
QT_QPA_PLATFORM=wayland ./build/PixelCanvas
```

**X11 (default on Ubuntu):**
```bash
QT_QPA_PLATFORM=xcb ./build/PixelCanvas
```

---

## Tablet setup

**Wacom (X11)**
```bash
sudo apt install xserver-xorg-input-wacom
```

**Huion / XP-Pen**
```bash
sudo apt install dkms
# then follow: https://github.com/DIGImend/digimend-kernel-drivers
```

Verify detection:
```bash
libinput list-devices   # check for tablet entry
xinput list             # X11 only
```

---

## Keyboard shortcuts

| Shortcut | Action |
|----------|--------|
| `[` / `]` | Decrease / increase brush size |
| `Ctrl+Z` / `Ctrl+Y` | Undo / Redo |
| `Ctrl+S` | Save project |
| `Ctrl+Shift+S` | Export flat PNG/JPEG |
| `B` | Brush tool |
| `E` | Eraser |
| `Space + drag` | Alternative pan |
| `Ctrl + Scroll` | Zoom |
| `Middle mouse drag` | Pan |

---

## Project file format (`.paint`)

ZIP archive containing:
- `project.json` — canvas size, layer metadata
- `layer_0.png`, `layer_1.png`, … — per-layer pixel data (ARGB32)

---

## Build phases

| Phase | What | Owner | Status |
|-------|------|-------|--------|
| 1–3 | Canvas window, pan/zoom, tablet input | Toma | skeleton done |
| 4 | Pixel brush (first drawing) | Toma | stub |
| 5 | Toolbar UI | Member 4 | skeleton done |
| 6–7 | Layer system + undo/redo | Toma + Member 4 | skeleton done |
| 8–9 | More brushes + texture tips | Toma | stub |
| 10 | Filters (OpenCV) | Toma + Member 4 | stub |
| 11 | libinput / XInput2 fallback | Toma | — |
| 12 | Save / Export (.paint format) | Toma + Member 4 | stub |
| 13 | Polish & performance | Toma | — |