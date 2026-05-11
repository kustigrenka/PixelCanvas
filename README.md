# PixelCanvas

**Professional Raster Painting Application for Linux**  
Built with C++20 · Qt6 · OpenCV

> A student-led engineering project — UFAZ (Université Franco-Azerbaïdjanaise) — 2026  
> *Tamilla Iskandarova · Madina Mammadova · Fakhriyya Huseynova · Farida Orujova*

---

## Overview

PixelCanvas is a Krita-inspired raster drawing application developed for Linux, targeting concept artists, illustrators, and digital painters who require a professional-grade, high-performance tool. It is written in modern C++20 with a Qt6 frontend, OpenCV-powered filters, and hardware-accelerated OpenGL rendering.

Pressure-sensitive stylus input is supported via the XInput2 protocol on X11, providing the low-latency tablet experience that professional artists depend on. The application ships with a full non-destructive layer system, ten brush tip types with wet-media simulation, a 100-step undo history, built-in speedpaint recording, and integrated reference browser panels — all in a single native Linux binary.

---

## Why PixelCanvas? A Professional Tool for Serious Artists

Microsoft Paint and similar consumer tools are built for casual use: flat colours, simple shapes, no awareness of drawing tablets, and no concept of layers. PixelCanvas was designed from the ground up around the workflow of a professional digital artist.

| Feature | Description |
|---|---|
| **Pressure sensitivity** | Every brush parameter — size, opacity, hardness — responds to stylus pressure via XInput2, giving artists the same expressiveness as physical media. Paint has no tablet support whatsoever. |
| **Layer system** | A full non-destructive layer stack with per-layer blend modes, opacity control, visibility toggle, drag reordering, and one-click duplication. |
| **Wet media simulation** | Wet Brush, Watercolour, and Marker tips sample and blend canvas colour during a stroke, producing blending behaviour impossible in flat-colour tools. |
| **Undo history** | A 100-step undo/redo stack with a scrubable history slider. Paint offers a single level of undo. |
| **OpenCV filters** | Non-destructive Gaussian Blur, Brightness/Contrast, Sharpen, and Invert filters implemented with the same library used in professional image processing pipelines. |
| **Session recording** | Built-in speedpaint recorder captures every stroke event with timing metadata for playback and export. |
| **Pose reference** | Integrated floating PoseMy.Art browser panel — reference and canvas visible simultaneously without alt-tabbing. |
| **Pinterest panel** | Integrated floating Pinterest browser for reference image browsing within the painting workflow. |
| **Scratch pad** | A persistent mini-canvas in the colour panel for testing brush strokes and mixing colours without touching the main canvas. |
| **Colour precision** | Full HSV colour wheel, six interactive RGB/HSV gradient sliders, 100-slot persistent swatch palette, and a live colour readout. |
| **Canvas operations** | New, Resize, Extend, and Crop dialogs with pixel-precise input and a 9-point anchor grid. Canvas sizes up to 16384×16384 px. |
| **Export** | Flat PNG/JPEG export plus the native `.paint` format (ZIP-archived per-layer PNGs + `project.json`) for full round-trip fidelity. |
| **Cloud backup** | Google Drive integration for uploading the current project directly from within the application via OAuth2. |

---

## Features

### Brush Engine

The brush engine supports ten tip types, each with its own rendering model:

- **Pixel** — hard-edged anti-aliased dab with shape mask support (circle, diamond, square)
- **Airbrush** — soft radial gradient with pressure-modulated opacity
- **Wet Brush** — samples canvas colour at stroke start and blends it with the painting colour
- **Watercolour** — dilution-based wet media with progressive softening
- **Marker** — semi-transparent flat colour with edge blending
- **Eraser / Selection Eraser** — alpha-cutting modes composited correctly via a scratch layer
- **Chalk** — noisy, granular dry-media texture
- **Smudge** — samples a region from the canvas and smears it forward
- **Blur** — progressive box-blur dab that softens the canvas under the stroke
- **TextureTip** — grayscale BMP/PNG shape mask plus a separate grayscale texture overlay for paper and grain effects

All dry-media tips (Pixel, Eraser, Chalk, TextureTip) use a scratch-layer compositing model that prevents overlapping dabs from stacking up opacity within a single stroke. The scratch layer is blended onto the canvas in a single `SourceOver` pass when the stroke ends.

Brush presets are stored in a 20-slot grid. Each slot persists its full `BrushSettings` including tip type, size, opacity, hardness, spacing, smoothing, blend mode, wet-media parameters, and any imported shape/texture bitmap paths. The active preset drives a live stroke-preview label that re-renders on every settings change.

### Canvas & Viewport

- OpenGL-accelerated compositing via `QOpenGLWidget`
- Zoom from 5% to 3200% via scroll wheel, pinch, or the Navigator slider
- Canvas rotation ±180° via the Navigator dock angle slider
- Horizontal flip toggle for mirror-drawing
- Pan with Space+drag or middle-mouse drag
- Colour eyedropper with Alt+click anywhere on the canvas
- Straight-line constraint with Shift+drag
- Batched dirty-rect recompositing at up to 120 fps via an 8 ms repaint timer

### Layer System

- Unlimited layers per document (memory-constrained)
- Drag-to-reorder in the Layers dock
- Per-layer blend modes: Normal, Multiply, Screen, Overlay, Add, Luminosity
- Per-layer opacity slider (0–100%) with numeric spin box
- Per-layer visibility toggle and duplicate layer
- All pixel data stored as `ARGB32_Premultiplied` to eliminate compositing halos

### Tablet & Input

- XInput2 (XI2) pressure-sensitive stylus support on X11 via `libXi`
- Pressure curves drive size, opacity, and wet-media parameters independently
- Tilt X/Y and barrel rotation captured per sample
- Configurable stroke smoothing (stabiliser) from 0 to 5, selectable in the Quick Bar
- Qt `tabletEvent()` suppressed when XI2 is active to prevent duplicate strokes
- Compile-time `PIXEL_CANVAS_XI2` guard: XI2 code compiles to nothing when `libXi` is absent, keeping Wayland builds clean

### Filters

- **Gaussian Blur** — radius 1–100 px via OpenCV `GaussianBlur`
- **Brightness / Contrast** — LUT-based: brightness −255–+255, contrast 0.0–3.0
- **Sharpen** — unsharp mask via `addWeighted`, strength 0.1–5.0
- **Invert** — RGB-only inversion, alpha preserved

All filters operate on a copy of the active layer. An undo snapshot is pushed before every filter application.

### Recording & Playback

- `StrokeRecorder` captures every pointer event with millisecond timestamps
- Each event stores the full `BrushSettings`, colour, and layer index at the time of the stroke
- `PlaybackWindow` replays the session in an isolated `BrushEngine`/`LayerStack` — the main canvas is never affected
- Variable playback speed (0.25× to 16×)
- Frame-by-frame PNG export for video production

### Google Drive Integration
 
- OAuth2 authentication flow handled by `GoogleDriveClient`
- Upload the current `.paint` project or a flat exported image directly to Google Drive without leaving the application


---

## Project Structure

```
pixel_canvas/
├── CMakeLists.txt
├── README.md
├── resources/
│   └── app.desktop                   XDG desktop entry for AppImage/Flatpak
└── src/
    ├── main.cpp                      Application entry point
    ├── canvas/
    │   ├── CanvasWidget.h/.cpp       OpenGL canvas: input, viewport, dirty-rect compositing, XI2
    │   ├── Stroke.h                  StrokeSample struct; Stroke = QVector<StrokeSample>
    │   └── StrokeRecorder.h          Timed stroke event recorder for speedpaint playback
    ├── brush/
    │   ├── BrushEngine.h/.cpp        Stroke pipeline: beginStroke / addSample / endStroke
    │   ├── BrushTip.h/.cpp           Abstract BrushTip + 10 concrete tip classes
    │   └── BrushSettings.h           BrushSettings POD, BrushPreset, TipType, BrushBlendMode
    ├── layers/
    │   ├── Layer.h                   Layer POD: pixels, name, opacity, blendMode, visible
    │   ├── LayerStack.h/.cpp         Owns all layers; recompositeRect(), filter dispatch
    │   └── UndoStack.h/.cpp          100-step snapshot-based undo/redo
    ├── filters/
    │   ├── Filter.h                  Abstract Filter interface
    │   └── FilterImpls.cpp           Blur, BrightnessContrast, Sharpen, Invert via OpenCV
    ├── googledrive/
    │   └── GoogleDriveClient.h/.cpp  OAuth2 authentication and flat-file upload to Google Drive
    ├── io/
    │   └── ProjectIO.h/.cpp          Save/load .paint (ZIP); flat PNG/JPEG export
    └── ui/
        ├── MainWindow.h/.cpp         Top-level window: menus, Quick Bar, docks, shortcuts
        ├── MainWindowCanvas.cpp      New / Resize / Extend / Crop canvas dialog slots
        ├── MainWindowDocks.cpp       Dock construction: Navigator, Layers, Color, Brushes
        ├── MainWindowFilters.cpp     Filter menu dialog slots
        ├── MainWindowPlayback.cpp    Recording toggle and PlaybackWindow launch
        ├── ToolbarPanel.h/.cpp       20-slot brush preset grid, sliders, live stroke preview
        ├── LayerPanel.h/.cpp         Layer list, blend mode, opacity, visibility
        ├── ColorPanelWidget.h/.cpp   Accordion panel: Wheel, Sliders, Swatches, Scratch Pad
        ├── ColorWheelWidget.h/.cpp   HSV colour wheel with hue ring and SV square
        ├── ColorSlidersWidget.h/.cpp Six interactive gradient-track sliders (R G B H S V)
        ├── ColorSwatchWidget.h/.cpp  100-slot persistent swatch palette
        ├── ScratchPadWidget.h/.cpp   Mini-canvas using the shared BrushEngine
        ├── PlaybackWindow.h/.cpp     Isolated playback engine, speed control, frame export
        ├── PinterestWindow.h/.cpp    Floating Qt WebEngine panel for Pinterest reference
        ├── MannequinWindow.h/.cpp    Floating Qt WebEngine panel for PoseMy.Art reference
        └── AppStyleSheet.h           SAI2-inspired dark theme
```

---

## Technical Specifications

| Component | Technology | Notes |
|---|---|---|
| Language | C++20 | `std::clamp`, designated initialisers, ranges |
| GUI framework | Qt 6 | Widgets, OpenGL, OpenGLWidgets, optional WebEngineWidgets |
| Rendering | OpenGL (`QOpenGLWidget`) | Hardware-accelerated compositing |
| Image processing | OpenCV 4 | Gaussian blur, LUT, unsharp mask, Mat ↔ QImage |
| Tablet input | XInput2 (`libXi`) | XI2 valuator pressure; Qt `tabletEvent()` fallback |
| Build system | CMake 3.20+ | Conditional `libXi` target, Qt6 private headers for `QZipWriter` |
| Compiler | GCC 12+ / Clang 15+ | C++20 required; tested on Ubuntu 22.04 and 24.04 |
| Project format | `.paint` (ZIP) | Per-layer PNGs + `project.json`; atomic writes via `QSaveFile` |
| Packaging | AppImage / Flatpak | XDG desktop entry included |
| Platform | Linux (X11 / Wayland) | Wayland `tablet_v2` protocol support planned |

---

## Downloading & Running (AppImage)

The easiest way to run PixelCanvas on Linux is via the prebuilt AppImage — no installation required.

### Download
Download the latest `PixelCanvas-x86_64.AppImage` from the [Releases](https://github.com/kustigrenka/PixelCanvas/releases) page.

### Run
```bash
chmod +x PixelCanvas-x86_64.AppImage
./PixelCanvas-x86_64.AppImage
```

That's it — no dependencies to install, no compilation needed.

### Building the AppImage yourself
If you want to build the AppImage from source:

```bash
# 1. Build the project
cmake -B build -S .
cmake --build build -j$(nproc)

# 2. Download linuxdeploy
wget https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage
wget https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage
chmod +x linuxdeploy-x86_64.AppImage linuxdeploy-plugin-qt-x86_64.AppImage

# 3. Create AppDir structure
mkdir -p AppDir/usr/bin AppDir/usr/share/applications AppDir/usr/share/icons/hicolor/256x256/apps
cp build/PixelCanvas AppDir/usr/bin/
cp resources/app.png AppDir/usr/share/icons/hicolor/256x256/apps/pixelcanvas.png

# 4. Build the AppImage
QMAKE=$(which qmake6) ./linuxdeploy-x86_64.AppImage \
  --appdir AppDir \
  --plugin qt \
  --output appimage
```

## Authors

Developed as a student-led engineering project at UFAZ (Université Franco-Azerbaïdjanaise), 2026.

| Name | |
|---|---|
| **Tamilla Iskandarova** | |
| **Madina Mammadova** | |
| **Fakhriyya Huseynova** | |
| **Farida Orujova** | |

---

*UFAZ — 2026 — All rights reserved*
