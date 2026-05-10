cmake_minimum_required(VERSION 3.25)
project(PixelCanvas)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets OpenGL OpenGLWidgets)
find_package(Qt6 OPTIONAL_COMPONENTS WebEngineWidgets)
find_package(OpenCV REQUIRED)
find_package(X11)
find_package(ZLIB REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets OpenGLWidgets Network)

add_executable(PixelCanvas
    src/main.cpp

    src/canvas/CanvasWidget.cpp
    src/brush/BrushEngine.cpp
    src/brush/BrushTip.cpp
    src/layers/LayerStack.cpp
    src/layers/UndoStack.cpp
    src/filters/FilterImpls.cpp
    src/io/ProjectIO.cpp

    src/ui/MainWindow.cpp
    src/ui/MainWindowDocks.cpp
    src/ui/MainWindowFilters.cpp
    src/ui/MainWindowCanvas.cpp
    src/ui/MainWindowPlayback.cpp
    src/ui/PlaybackWindow.cpp
    src/ui/ToolbarPanel.cpp
    src/ui/LayerPanel.cpp
    src/ui/ColorWheelWidget.cpp
    src/ui/ColorSlidersWidget.cpp
    src/ui/ColorSwatchWidget.cpp
    src/ui/ColorPanelWidget.cpp
    src/ui/ScratchPadWidget.cpp
    src/ui/PinterestWindow.cpp
    src/ui/MannequinWindow.cpp

    src/GoogleDrive/GoogleDriveClient.cpp
)

qt_add_resources(PixelCanvas "resources"
    PREFIX "/"
    FILES resources/app.png
)

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
    Qt6::Network
)

if(Qt6WebEngineWidgets_FOUND)
    target_compile_definitions(PixelCanvas PRIVATE QT_WEBENGINEWIDGETS_LIB)
    target_link_libraries(PixelCanvas PRIVATE Qt6::WebEngineWidgets)
    target_compile_definitions(PixelCanvas PRIVATE
        QTWEBENGINEPROCESS_PATH="/usr/lib/qt6/libexec/QtWebEngineProcess")
    message(STATUS "Qt WebEngine found — Pinterest embedded browser enabled")
else()
    message(STATUS "Qt WebEngine NOT found — using system browser fallback")
endif()