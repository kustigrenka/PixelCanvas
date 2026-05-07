cmake_minimum_required(VERSION 3.25)
project(PixelCanvas)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets OpenGL OpenGLWidgets)
find_package(OpenCV REQUIRED)
find_package(X11)   # provides X11::Xi for XInput2
find_package(ZLIB REQUIRED)

add_executable(PixelCanvas
    # ── Entry point ───────────────────────────────────────────────────────────
    src/main.cpp

    # ── Core (Toma) ───────────────────────────────────────────────────────────
    src/CanvasWidget.cpp
    src/BrushEngine.cpp
    src/BrushTip.cpp
    src/BrushPreviewWidget.cpp      # ← new: extracted from ToolbarPanel.cpp
    src/LayerStack.cpp
    src/UndoStack.cpp
    src/FilterImpls.cpp
    src/ProjectIO.cpp

    # ── UI (Member 4) ─────────────────────────────────────────────────────────
    src/MainWindow.cpp              # core wiring only
    src/MainWindowDocks.cpp         # ← new: buildDocks() + dock-content builders
    src/MainWindowFilters.cpp       # ← new: filter dialog slots
    src/MainWindowCanvas.cpp        # ← new: canvas operation dialog slots
    src/ToolbarPanel.cpp
    src/LayerPanel.cpp
    src/ColorWheelWidget.cpp
)

target_link_libraries(PixelCanvas PRIVATE
    Qt6::Widgets
    Qt6::OpenGL
    Qt6::OpenGLWidgets
    ${OpenCV_LIBS}
    $<$<BOOL:${X11_Xi_FOUND}>:X11::Xi>
    ZLIB::ZLIB
)
