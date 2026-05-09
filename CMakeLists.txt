cmake_minimum_required(VERSION 3.25)
project(PixelCanvas)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets OpenGL OpenGLWidgets)
find_package(OpenCV REQUIRED)
find_package(X11)
find_package(ZLIB REQUIRED)

add_executable(PixelCanvas
    # ── Entry point ──────────────────────────────────────────────────────────
    src/main.cpp

    # ── Core (Toma) ──────────────────────────────────────────────────────────
    src/canvas/CanvasWidget.cpp
    src/brush/BrushEngine.cpp
    src/brush/BrushTip.cpp
    src/layers/LayerStack.cpp
    src/layers/UndoStack.cpp
    src/filters/FilterImpls.cpp
    src/io/ProjectIO.cpp

    # ── UI (Member 4) ────────────────────────────────────────────────────────
    src/ui/MainWindow.cpp
    src/ui/MainWindowDocks.cpp
    src/ui/MainWindowFilters.cpp
    src/ui/MainWindowCanvas.cpp
    src/ui/ToolbarPanel.cpp
    src/ui/LayerPanel.cpp
    src/ui/ColorWheelWidget.cpp
    src/ui/ColorSlidersWidget.cpp
    src/ui/ColorSwatchWidget.cpp
    src/ui/ColorPanelWidget.cpp
    src/ui/ScratchPadWidget.cpp
)

# ── Include dirs: lets all files use flat #include "X.h" with no path changes ──
target_include_directories(PixelCanvas PRIVATE
    src
    src/canvas
    src/brush
    src/layers
    src/filters
    src/io
    src/ui
)

target_link_libraries(PixelCanvas PRIVATE
    Qt6::Widgets
    Qt6::OpenGL
    Qt6::OpenGLWidgets
    Qt6::Core
    ${OpenCV_LIBS}
    $<$<BOOL:${X11_Xi_FOUND}>:X11::Xi>
    ZLIB::ZLIB
)