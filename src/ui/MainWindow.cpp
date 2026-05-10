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
#include <QCloseEvent>


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
#include "PinterestWindow.h"
#include "MannequinWindow.h"

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
    file->addAction(tr("&Import Image…"), this, &MainWindow::onImportPng,
                    QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_I));
    file->addSeparator();
    file->addAction(tr("&Save"),         this, &MainWindow::onSave,        QKeySequence::Save);
    file->addAction(tr("Save &As…"),     this, &MainWindow::onSaveAs,      QKeySequence::SaveAs);
    file->addAction(tr("&Export Flat…"), this, &MainWindow::onExportFlat,
                    QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_S));
    file->addAction(tr("&Import Image…"), this, &MainWindow::onImportPng,
                    QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_I));
                    
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

    // ── References ────────────────────────────────────────────────────────────
    auto *references = menuBar()->addMenu(tr("&References"));

    references->addAction(tr("⬡  Open Pinterest…"), this, [this]() {
        if (!m_pinterestWin)
            m_pinterestWin = new PinterestWindow(this);
        m_pinterestWin->show();
        m_pinterestWin->raise();
        m_pinterestWin->activateWindow();
    });

    references->addAction(tr("◈  Open Pose Reference (PoseMy.Art)…"), this, [this]() {
        if (!m_mannequinWin)
            m_mannequinWin = new MannequinWindow(this, "https://posemy.art/");
        m_mannequinWin->show();
        m_mannequinWin->raise();
        m_mannequinWin->activateWindow();
    });

    references->addSeparator();

    references->addAction(tr("Close Pinterest"), this, [this]() {
        if (m_pinterestWin && m_pinterestWin->isVisible())
            m_pinterestWin->hide();
    });

    references->addAction(tr("Close Mannequins"), this, [this]() {
        if (m_mannequinWin && m_mannequinWin->isVisible())
            m_mannequinWin->hide();
    });

    // ── Window (dock toggles added later in buildDocks) ────────────────────
    menuBar()->addMenu(tr("&Window"));

    // playback

    QMenu *playbackMenu = menuBar()->addMenu(tr("&Playback"));
    m_recordAction = playbackMenu->addAction(tr("⏺  Start Recording"));
    m_recordAction->setCheckable(true);
    m_recordAction->setShortcut(QKeySequence(tr("Ctrl+Shift+R")));
    connect(m_recordAction, &QAction::triggered, this, &MainWindow::onToggleRecording);

    m_playAction = playbackMenu->addAction(tr("▶  Play Recording"));
    m_playAction->setShortcut(QKeySequence(tr("Ctrl+Shift+P")));
    m_playAction->setEnabled(false);
    connect(m_playAction, &QAction::triggered, this, &MainWindow::onPlayback);

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
    m_quickBar->addSeparator();

    auto *histLabel = new QLabel(tr(" History:"), this);
    m_quickBar->addWidget(histLabel);

    m_undoSlider = new QSlider(Qt::Horizontal, this);
    m_undoSlider->setFixedWidth(120);
    m_undoSlider->setRange(0, 0);
    m_undoSlider->setValue(0);
    m_undoSlider->setToolTip(tr("Drag to scrub through undo history"));
    m_quickBar->addWidget(m_undoSlider);

    connect(m_undoSlider, &QSlider::valueChanged,
            this, &MainWindow::onUndoSliderMoved);

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

    connect(m_undoStack, &UndoStack::historyChanged, this, [this]() {
        const QSignalBlocker b(m_undoSlider);
        m_undoSlider->setRange(0, qMax(0, m_undoStack->historySize() - 1));
        m_undoSlider->setValue(m_undoStack->currentIndex());
    });

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
    const Snapshot s = m_undoStack->undo();
    if (Layer *l = m_layerStack->layerAt(s.layerIndex))
        l->pixels = s.pixels;
    m_layerStack->setActiveLayer(s.layerIndex);
    m_canvas->forceRecomposite();
}

void MainWindow::onRedo()
{
    if (!m_undoStack->canRedo()) return;
    const Snapshot s = m_undoStack->redo();
    if (Layer *l = m_layerStack->layerAt(s.layerIndex))
        l->pixels = s.pixels;
    m_layerStack->setActiveLayer(s.layerIndex);
    m_canvas->forceRecomposite();
}

// ─────────────────────────────────────────────────────────────────────────────
// File slots
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::onOpen()
{
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Open"), {},
        tr("All Supported (*.paint *.png *.jpg *.jpeg);;"
           "PixelCanvas Projects (*.paint);;"
           "Images (*.png *.jpg *.jpeg)"));
    if (path.isEmpty()) return;

    const QString ext = QFileInfo(path).suffix().toLower();
    if (ext == QLatin1String("paint")) {
        if (!m_projectIO->loadProject(path)) {
            QMessageBox::warning(this, tr("Open Failed"),
                                 tr("Could not open project file."));
            return;
        }
        m_currentFile = path;
        m_canvas->reinitCanvas();
        m_canvas->forceRecomposite();
        setWindowTitle(tr("PixelCanvas — %1").arg(QFileInfo(path).fileName()));
    } else {
        importPng(path);   // PNG / JPEG path
    }
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

void MainWindow::importPng(const QString &path)
{
    QImage img(path);
    if (img.isNull()) {
        QMessageBox::warning(this, tr("Import Failed"),
                             tr("Could not read image file."));
        return;
    }

    // Convert to the internal format
    if (img.format() != QImage::Format_ARGB32_Premultiplied)
        img = img.convertToFormat(QImage::Format_ARGB32_Premultiplied);

    // Re-initialise the layer stack at the image's size
    m_layerStack->init(img.size());

    // Put the image into the background layer (index 0)
    Layer *bg = m_layerStack->layerAt(0);
    bg->pixels = img;
    bg->name   = QFileInfo(path).baseName();

    // Reinit the canvas to match the new size, then repaint
    m_canvas->reinitCanvas();
    m_canvas->forceRecomposite();

    m_currentFile.clear();   // it's an import, not a .paint project
    setWindowTitle(tr("PixelCanvas — %1 [imported]")
                   .arg(QFileInfo(path).fileName()));
}

void MainWindow::onImportPng()
{
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Import Image"), {},
        tr("Images (*.png *.jpg *.jpeg *.bmp *.tiff)"));
    if (!path.isEmpty()) importPng(path);
}

bool MainWindow::maybeSave()
{
    if (!m_canvas->isDirty()) return true;
    const auto btn = QMessageBox::question(this, tr("Unsaved Changes"),
        tr("The canvas has unsaved changes. Save before continuing?"),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
    if (btn == QMessageBox::Save)    { onSave(); return true; }
    if (btn == QMessageBox::Discard) { return true; }
    return false;
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (!maybeSave()) { event->ignore(); return; }
    if (m_toolbar) m_toolbar->saveSettings();
    event->accept();
}

void MainWindow::onUndoSliderMoved(int value)
{
    if (m_undoStack->historySize() == 0) return;
    if (value == m_undoStack->currentIndex()) return;

    const int current = m_undoStack->currentIndex();
    if (value < current) {
        for (int i = current; i > value && m_undoStack->canUndo(); --i)
            m_undoStack->undo();
    } else {
        for (int i = current; i < value && m_undoStack->canRedo(); ++i)
            m_undoStack->redo();
    }

    const Snapshot final = m_undoStack->snapshotAt(m_undoStack->currentIndex());
    if (Layer *l = m_layerStack->layerAt(final.layerIndex))
        l->pixels = final.pixels;
    m_layerStack->setActiveLayer(final.layerIndex);
    m_canvas->forceRecomposite();
}