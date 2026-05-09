// ─────────────────────────────────────────────────────────────────────────────
// MainWindow.cpp  –  core wiring: constructor, menu bar, quick bar,
//                    status bar, signal connections, undo/redo, file I/O.
//
// The following groups live in their own translation units to keep this file
// focused:
//   MainWindowDocks.cpp   – buildDocks() + buildNavigator/Color/Brush/Preview
//   MainWindowFilters.cpp – onFilterBlur/BrightnessContrast/Invert/Sharpen
//   MainWindowCanvas.cpp  – onNewCanvas/CanvasResize/CanvasExtend/CanvasCrop
// ─────────────────────────────────────────────────────────────────────────────
#include "MainWindow.h"

#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QFileDialog>
#include <QFileInfo>
#include <QStandardPaths>
#include <QSpinBox>
#include <QComboBox>
#include <QLabel>
#include <QKeySequence>
#include <QShortcut>
#include <QMessageBox>
#include <QApplication>
#include <QSettings>


#include "CanvasWidget.h"
#include "ToolbarPanel.h"
#include "LayerPanel.h"
#include "BrushEngine.h"
#include "LayerStack.h"
#include "ProjectIO.h"
#include "UndoStack.h"
#include "Layer.h"
#include "AppStyleSheet.h"
#include "ColorPanelWidget.h"


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    qApp->setStyleSheet(kAppStyleSheet);

    setWindowTitle("PixelCanvas");
    resize(1400, 900);
    setDockOptions(QMainWindow::AnimatedDocks | QMainWindow::AllowTabbedDocks);
    setObjectName("MainWindow");

    // ── Core objects ──────────────────────────────────────────────────────────
    m_layerStack  = new LayerStack(this);
    m_brushEngine = new BrushEngine(this);
    m_projectIO   = new ProjectIO(this);
    m_projectIO->setLayerStack(m_layerStack);
    m_undoStack   = new UndoStack(this);

    // ── Central canvas ────────────────────────────────────────────────────────
    m_canvas = new CanvasWidget(m_layerStack, m_brushEngine, m_undoStack, this);
    setCentralWidget(m_canvas);

    // ── UI shell ──────────────────────────────────────────────────────────────
    buildMenuBar();
    buildQuickBar();
    buildDocks();
    QSettings s("YourOrg", "YourApp");
    restoreGeometry(s.value("window/geometry").toByteArray());
    restoreState(s.value("window/state").toByteArray());        // defined in MainWindowDocks.cpp
    buildStatusBar();
    connectSignals();
    applyShortcuts();
}

MainWindow::~MainWindow() = default;

// ─────────────────────────────────────────────────────────────────────────────
// Menu bar  –  File · Edit · View · Layer · Canvas · Filter · Window · Help
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::buildMenuBar()
{
    // ── File ──────────────────────────────────────────────────────────────────
    auto *file = menuBar()->addMenu(tr("&File"));
    file->addAction(tr("&New"),          this, &MainWindow::onNewCanvas,   QKeySequence::New);
    file->addAction(tr("&Open…"),        this, &MainWindow::onOpen,        QKeySequence::Open);
    file->addSeparator();
    file->addAction(tr("&Save"),         this, &MainWindow::onSave,        QKeySequence::Save);
    file->addAction(tr("Save &As…"),     this, &MainWindow::onSaveAs,      QKeySequence::SaveAs);
    file->addAction(tr("&Export Flat…"), this, &MainWindow::onExportFlat,
                    QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_S));
    file->addSeparator();
    file->addAction(tr("&Quit"), this, &QWidget::close, QKeySequence::Quit);

    // ── Edit ──────────────────────────────────────────────────────────────────
    auto *edit = menuBar()->addMenu(tr("&Edit"));
    m_undoAction = edit->addAction(tr("&Undo"), this, &MainWindow::onUndo, QKeySequence::Undo);
    m_redoAction = edit->addAction(tr("&Redo"), this, &MainWindow::onRedo);
    m_redoAction->setShortcuts({QKeySequence(Qt::CTRL | Qt::Key_Y),
                                QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Z)});
    m_undoAction->setEnabled(false);
    m_redoAction->setEnabled(false);

    // ── View ──────────────────────────────────────────────────────────────────
    auto *view = menuBar()->addMenu(tr("&View"));
    view->addAction(tr("Zoom In"),       m_canvas,
                    [this]{ m_canvas->setZoom(m_canvas->zoom() * 1.25f); },
                    QKeySequence(Qt::CTRL | Qt::Key_Equal));
    view->addAction(tr("Zoom Out"),      m_canvas,
                    [this]{ m_canvas->setZoom(m_canvas->zoom() * 0.8f); },
                    QKeySequence(Qt::CTRL | Qt::Key_Minus));
    view->addAction(tr("Fit to Window"), m_canvas, &CanvasWidget::resetView,
                    QKeySequence(Qt::CTRL | Qt::Key_0));
    view->addSeparator();
    view->addAction(tr("Flip &Horizontal"), this, &MainWindow::onFlipH,
                    QKeySequence(Qt::Key_H));

    // ── Layer ─────────────────────────────────────────────────────────────────
    auto *layer = menuBar()->addMenu(tr("&Layer"));
    layer->addAction(tr("New Layer"), this, [this]{
        m_layerStack->addLayer(QString("Layer %1").arg(m_layerStack->count() + 1));
    });
    layer->addAction(tr("Delete Layer"), this, [this]{
        m_layerStack->removeLayer(m_layerStack->activeIndex());
    });
    layer->addAction(tr("Duplicate Layer"), this, [this]{
        const int newIdx = m_layerStack->duplicateLayer(m_layerStack->activeIndex());
        if (newIdx >= 0) m_layerStack->setActiveLayer(newIdx);
    });

    // ── Canvas ────────────────────────────────────────────────────────────────
    auto *canvas = menuBar()->addMenu(tr("&Canvas"));
    canvas->addAction(tr("&New Canvas…"),    this, &MainWindow::onNewCanvas);
    canvas->addAction(tr("&Resize Canvas…"), this, &MainWindow::onCanvasResize);
    canvas->addAction(tr("&Extend Canvas…"), this, &MainWindow::onCanvasExtend);
    canvas->addAction(tr("&Crop Canvas…"),   this, &MainWindow::onCanvasCrop);

    // ── Filter ────────────────────────────────────────────────────────────────
    auto *filter = menuBar()->addMenu(tr("F&ilter"));
    filter->addAction(tr("&Blur…"),                this, &MainWindow::onFilterBlur);
    filter->addAction(tr("&Brightness/Contrast…"), this, &MainWindow::onFilterBrightnessContrast);
    filter->addAction(tr("&Invert"),               this, &MainWindow::onFilterInvert);
    filter->addAction(tr("&Sharpen…"),             this, &MainWindow::onFilterSharpen);

    // ── Window (dock toggles added later in buildDocks) ────────────────────
    menuBar()->addMenu(tr("&Window"));

    // ── Help ──────────────────────────────────────────────────────────────────
    menuBar()->addMenu(tr("&Help"));
}

// ─────────────────────────────────────────────────────────────────────────────
// Quick bar  –  Undo/Redo · Flip H · Zoom% · Stabilizer S-0…S-7
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::buildQuickBar()
{
    m_quickBar = new QToolBar(tr("Quick Bar"), this);
    m_quickBar->setMovable(false);
    m_quickBar->setIconSize(QSize(16, 16));
    addToolBar(Qt::TopToolBarArea, m_quickBar);

    m_quickBar->addAction(tr("↩ Undo"), this, &MainWindow::onUndo);
    m_quickBar->addAction(tr("↪ Redo"), this, &MainWindow::onRedo);
    m_quickBar->addSeparator();

    m_quickBar->addAction(tr("⇄ Flip H"), this, &MainWindow::onFlipH);
    m_quickBar->addSeparator();

    m_quickBar->addWidget(new QLabel(tr("Zoom:"), this));
    m_zoomSpin = new QSpinBox(this);
    m_zoomSpin->setRange(5, 3200);
    m_zoomSpin->setValue(100);
    m_zoomSpin->setSuffix(tr("%"));
    m_zoomSpin->setFixedWidth(72);
    connect(m_zoomSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &MainWindow::onQuickZoomChanged);
    m_quickBar->addWidget(m_zoomSpin);
    m_quickBar->addAction(tr("1:1"), this, &MainWindow::onResetZoom);
    m_quickBar->addSeparator();

    m_quickBar->addWidget(new QLabel(tr("Stabilizer:"), this));
    m_stabCombo = new QComboBox(this);
    for (int i = 0; i <= 7; ++i)
        m_stabCombo->addItem(QString("S-%1").arg(i));
    m_stabCombo->setFixedWidth(56);
    connect(m_stabCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onStabilizerChanged);
    m_quickBar->addWidget(m_stabCombo);
}

// ─────────────────────────────────────────────────────────────────────────────
// Status bar  –  Tool · Coords · Pressure · Zoom
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::buildStatusBar()
{
    m_statusTool     = new QLabel(tr("Pen · 20 px"), this);
    m_statusCoord    = new QLabel(tr("X: 0  Y: 0"), this);
    m_statusPressure = new QLabel(tr("Pressure: 0.00"), this);
    m_statusZoom     = new QLabel(tr("Zoom: 100%  ·  Rotation: 0°"), this);

    statusBar()->addWidget(m_statusTool);
    statusBar()->addWidget(new QLabel("|", this));
    statusBar()->addWidget(m_statusCoord);
    statusBar()->addWidget(new QLabel("|", this));
    statusBar()->addWidget(m_statusPressure);
    statusBar()->addPermanentWidget(m_statusZoom);
}

// ─────────────────────────────────────────────────────────────────────────────
// Signal wiring
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::connectSignals()
{
    connect(m_toolbar, &ToolbarPanel::brushSettingsChanged,
            m_brushEngine, &BrushEngine::setSettings);
    connect(m_toolbar, &ToolbarPanel::colorChanged,
            m_brushEngine, &BrushEngine::setColor);

    connect(m_toolbar, &ToolbarPanel::brushSettingsChanged,
            this, [this](const BrushSettings &s) {
        m_statusTool->setText(QString("Pen · %1 px").arg(static_cast<int>(s.size)));
    });

    connect(m_canvas, &CanvasWidget::cursorMoved,  this, &MainWindow::onCursorMoved);
    connect(m_canvas, &CanvasWidget::zoomChanged,  this, &MainWindow::onZoomChanged);

    connect(m_undoStack, &UndoStack::undoAvailable, m_undoAction, &QAction::setEnabled);
    connect(m_undoStack, &UndoStack::redoAvailable, m_redoAction, &QAction::setEnabled);

    connect(m_layerStack, &LayerStack::layerPropertiesChanged,
            m_canvas, &CanvasWidget::forceRecomposite);

    connect(m_canvas, &CanvasWidget::colorPicked,
            m_toolbar, &ToolbarPanel::onExternalColorChanged);

    connect(m_colorPanel, &ColorPanelWidget::colorChanged,
        m_toolbar, &ToolbarPanel::onExternalColorChanged);
}

// ─────────────────────────────────────────────────────────────────────────────
// Keyboard shortcuts not tied to menu actions
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::applyShortcuts()
{
    new QShortcut(QKeySequence(Qt::Key_B), this, [this]{
        m_toolbar->selectTool(ToolbarPanel::Tool::Brush);
    });
    new QShortcut(QKeySequence(Qt::Key_E), this, [this]{
        m_toolbar->selectTool(ToolbarPanel::Tool::Eraser);
    });
    new QShortcut(QKeySequence(Qt::Key_I), this, [this]{
        m_toolbar->selectTool(ToolbarPanel::Tool::Eyedropper);
    });
}

// ─────────────────────────────────────────────────────────────────────────────
// Status bar update slots
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::onCursorMoved(QPointF pos, float pressure)
{
    m_statusCoord->setText(
        QString("X: %1  Y: %2").arg(static_cast<int>(pos.x()))
                               .arg(static_cast<int>(pos.y())));
    m_statusPressure->setText(QString("Pressure: %1").arg(pressure, 0, 'f', 2));
}

void MainWindow::onZoomChanged(float zoom)
{
    const int pct = static_cast<int>(zoom * 100.0f);
    m_statusZoom->setText(QString("Zoom: %1%  ·  Rotation: 0°").arg(pct));
    const QSignalBlocker b(m_zoomSpin);
    m_zoomSpin->setValue(pct);
}

// ─────────────────────────────────────────────────────────────────────────────
// Quick bar slots
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::onQuickZoomChanged(int percent)
{
    m_canvas->setZoom(percent / 100.0f);
}

void MainWindow::onResetZoom()
{
    m_canvas->resetView();
}

void MainWindow::onFlipH()
{
    m_canvas->setFlipH(!m_canvas->flipH());
}

void MainWindow::onStabilizerChanged(int level)
{
    BrushSettings s = m_brushEngine->settings();
    s.smoothing = level / 8.0f;
    m_brushEngine->setSettings(s);
}

// ─────────────────────────────────────────────────────────────────────────────
// Undo / Redo
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::onUndo()
{
    if (!m_undoStack->canUndo()) return;
    const int snapLayer = m_undoStack->peekUndoLayerIndex();
    Layer *l = m_layerStack->layerAt(snapLayer);
    if (!l) return;
    Snapshot s = m_undoStack->undo(snapLayer, l->pixels);
    m_layerStack->layerAt(s.layerIndex)->pixels = s.pixels;
    m_canvas->forceRecomposite();
}

void MainWindow::onRedo()
{
    if (!m_undoStack->canRedo()) return;
    const int snapLayer = m_undoStack->peekRedoLayerIndex();
    Layer *l = m_layerStack->layerAt(snapLayer);
    if (!l) return;
    Snapshot s = m_undoStack->redo(snapLayer, l->pixels);
    m_layerStack->layerAt(s.layerIndex)->pixels = s.pixels;
    m_canvas->forceRecomposite();
}

// ─────────────────────────────────────────────────────────────────────────────
// File slots
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::onOpen()
{
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Open Project"), {}, tr("PixelCanvas Projects (*.paint)"));
    if (path.isEmpty()) return;
    if (!m_projectIO->loadProject(path)) {
        QMessageBox::warning(this, tr("Open Failed"),
                             tr("Could not open project file."));
        return;
    }
    m_currentFile = path;
    m_canvas->reinitCanvas();
    m_canvas->forceRecomposite();
    setWindowTitle(tr("PixelCanvas — %1").arg(QFileInfo(path).fileName()));
}

void MainWindow::onSave()
{
    if (m_currentFile.isEmpty()) { onSaveAs(); return; }
    if (!m_projectIO->saveProject(m_currentFile))
        QMessageBox::warning(this, tr("Save Failed"), tr("Could not save project file."));
}

void MainWindow::onSaveAs()
{
    const QString dir = QStandardPaths::writableLocation(
        QStandardPaths::AppLocalDataLocation);
    QString path = QFileDialog::getSaveFileName(
        this, tr("Save Project"), dir, tr("PixelCanvas Projects (*.paint)"));
    if (path.isEmpty()) return;
    if (!path.endsWith(QLatin1String(".paint"), Qt::CaseInsensitive))
        path += QStringLiteral(".paint");
    m_currentFile = path;
    if (!m_projectIO->saveProject(path))
        QMessageBox::warning(this, tr("Save Failed"), tr("Could not save project file."));
    else
        setWindowTitle(tr("PixelCanvas — %1").arg(QFileInfo(path).fileName()));
}

void MainWindow::onExportFlat()
{
    QString path = QFileDialog::getSaveFileName(
        this, tr("Export Flat Image"), {},
        tr("PNG Image (*.png);;JPEG Image (*.jpg *.jpeg)"));
    if (path.isEmpty()) return;
    if (!path.endsWith(QLatin1String(".png"),  Qt::CaseInsensitive) &&
        !path.endsWith(QLatin1String(".jpg"),  Qt::CaseInsensitive) &&
        !path.endsWith(QLatin1String(".jpeg"), Qt::CaseInsensitive))
        path += QStringLiteral(".png");
    m_projectIO->exportFlat(path);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    QSettings s("YourOrg", "YourApp");
    s.setValue("window/geometry", saveGeometry());
    s.setValue("window/state",    saveState());
    QMainWindow::closeEvent(event);
}
