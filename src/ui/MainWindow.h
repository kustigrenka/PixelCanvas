#pragma once

#include <QMainWindow>
#include <QLabel>

#include "BrushSettings.h"   // needed for onStabilizerChanged signature

class CanvasWidget;
class ToolbarPanel;
class LayerPanel;
class BrushEngine;
class LayerStack;
class ProjectIO;
class UndoStack;
class Filter;
class QToolBar;
class QComboBox;
class QSpinBox;
class QSlider;  
class QDockWidget;
class QAction;
class ColorWheelWidget;
class ColorPanelWidget;
class PinterestWindow;
class MannequinWindow;
class GoogleDriveClient;
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

public slots:
    void onCursorMoved(QPointF canvasPos, float pressure);
    void onZoomChanged(float zoom);

private slots:
    // File
    void onNewCanvas();
    void onCanvasResize();
    void onCanvasExtend();
    void onCanvasCrop();
    void onOpen();
    void onSave();
    void onSaveAs();
    void onExportFlat();

    void onImportPng(); 
    void onUploadToDrive();
    void onGDriveAuthenticated();
    void onGDriveAuthFailed(const QString &msg);
    void onGDriveUploadFinished(bool ok, const QString &fileName);

    // Edit
    void onUndo();
    void onRedo();

    // Filter
    void onFilterBlur();
    void onFilterBrightnessContrast();
    void onFilterInvert();
    void onFilterSharpen();

    // Quick bar
    void onQuickZoomChanged(int percent);
    void onResetZoom();
    void onFlipH();
    void onStabilizerChanged(int level);

    void onToggleRecording();
    void onPlayback();
    void onUndoSliderMoved(int value);
    
protected:
    void closeEvent(QCloseEvent *event) override;

private:
    // ── Setup helpers (defined across the four .cpp files) ────────────────────
    void buildMenuBar();
    void buildQuickBar();
    void buildDocks();           // MainWindowDocks.cpp
    void buildStatusBar();
    void connectSignals();
    void applyShortcuts();

    void importPng(const QString &path);   // ← add this

    // Dock-content builders    (MainWindowDocks.cpp)
    QWidget *buildNavigatorWidget();
    QWidget *buildColorWidget();
    QWidget *buildBrushWidget();

    // ── Speedpaint / Playback ─────────────────────────────────────────────────
    QAction *m_recordAction  = nullptr;
    QAction *m_playAction    = nullptr;

    // ── Core objects ──────────────────────────────────────────────────────────
    LayerStack    *m_layerStack   = nullptr;
    BrushEngine   *m_brushEngine  = nullptr;
    ProjectIO     *m_projectIO    = nullptr;
    UndoStack     *m_undoStack    = nullptr;
    GoogleDriveClient *m_driveClient = nullptr;

    // ── Widgets ───────────────────────────────────────────────────────────────
    CanvasWidget  *m_canvas       = nullptr;
    ToolbarPanel  *m_toolbar      = nullptr;
    LayerPanel    *m_layerPanel   = nullptr;
    ColorPanelWidget  *m_colorPanel   = nullptr;


    // ── Docks ─────────────────────────────────────────────────────────────────
    QDockWidget   *m_dockNavigator = nullptr;
    QDockWidget   *m_dockColor     = nullptr;
    QDockWidget   *m_dockBrushes   = nullptr;
    QDockWidget   *m_dockLayers    = nullptr;

    // ── References floating windows (created lazily on first open) ────────────
    PinterestWindow *m_pinterestWin = nullptr;
    MannequinWindow *m_mannequinWin = nullptr;

    // ── Quick bar widgets ─────────────────────────────────────────────────────
    QToolBar      *m_quickBar     = nullptr;
    QSpinBox      *m_zoomSpin     = nullptr;
    QComboBox     *m_stabCombo    = nullptr;
    QSlider *m_undoSlider = nullptr;

    // ── Edit actions ──────────────────────────────────────────────────────────
    QAction       *m_undoAction   = nullptr;
    QAction       *m_redoAction   = nullptr;
    QAction *m_driveAction = nullptr;

    // ── Status bar labels ─────────────────────────────────────────────────────
    QLabel        *m_statusTool     = nullptr;
    QLabel        *m_statusCoord    = nullptr;
    QLabel        *m_statusPressure = nullptr;
    QLabel        *m_statusZoom     = nullptr;

    QString        m_currentFile;

    bool maybeSave();
};