// ─────────────────────────────────────────────────────────────────────────────
// MainWindowDocks.cpp
//
// Dock construction for MainWindow: buildDocks() and the four dock-content
// builder helpers (Navigator, Color, Brush, Preview).
//
// Kept separate from MainWindow.cpp so the large widget-building code doesn't
// clutter the core wiring logic.
// ─────────────────────────────────────────────────────────────────────────────
#include "MainWindow.h"

#include <QDockWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QSlider>
#include <QToolButton>
#include <QScrollArea>
#include <QFrame>
#include <QPainter>
#include <QMenuBar>
#include <QMenu>

#include "CanvasWidget.h"
#include "ToolbarPanel.h"
#include "LayerPanel.h"
#include "LayerStack.h"
#include "ColorPanelWidget.h"
#include "BrushEngine.h"

// ─────────────────────────────────────────────────────────────────────────────
// buildDocks  –  five independent docks
//
//   LEFT side (default)
//     • Navigator  – canvas thumbnail + scale/angle sliders
//     • Layers     – layer list + blend/opacity controls
//
//   RIGHT side (default)
//     • Color      – ColorWheelWidget + primary/secondary swatches
//     • Brushes    – brush type tabs + preset list + settings sliders
//     • Preview    – live stroke preview
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::buildDocks()
{
    m_toolbar = new ToolbarPanel(m_brushEngine, this);
    m_layerPanel = new LayerPanel(m_layerStack, this);

    auto makeDock = [this](const QString &title,
                           QWidget       *inner,
                           Qt::DockWidgetArea area,
                           QDockWidget   *&out,
                           int             minW = 200) {
        out = new QDockWidget(title, this);
        out->setWidget(inner);
        out->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
        out->setMinimumWidth(minW);
        addDockWidget(area, out);
    };

    // ── Left docks ────────────────────────────────────────────────────────────
    makeDock(tr("Navigator"), buildNavigatorWidget(),
             Qt::LeftDockWidgetArea,  m_dockNavigator, 180);

    makeDock(tr("Layers"),    m_layerPanel,
             Qt::LeftDockWidgetArea,  m_dockLayers,    180);

    // ── Right docks ───────────────────────────────────────────────────────────
    makeDock(tr("Color"),     buildColorWidget(),
             Qt::RightDockWidgetArea, m_dockColor,     220);

    makeDock(tr("Brushes"),   buildBrushWidget(),
             Qt::RightDockWidgetArea, m_dockBrushes,   220);

    // ── Object names (required for restoreState) ──────────────────────────────
    m_dockNavigator->setObjectName("dockNavigator");
    m_dockLayers->setObjectName("dockLayers");
    m_dockColor->setObjectName("dockColor");
    m_dockBrushes->setObjectName("dockBrushes");

    splitDockWidget(m_dockNavigator, m_dockLayers,   Qt::Vertical);
    splitDockWidget(m_dockColor,     m_dockBrushes,  Qt::Vertical);

    // Populate Window menu
    for (QObject *obj : menuBar()->children()) {
        if (auto *menu = qobject_cast<QMenu *>(obj)) {
            if (menu->title() == tr("&Window")) {
                menu->addAction(m_dockNavigator->toggleViewAction());
                menu->addAction(m_dockLayers->toggleViewAction());
                menu->addSeparator();
                menu->addAction(m_dockColor->toggleViewAction());
                menu->addAction(m_dockBrushes->toggleViewAction());
                break;
            }
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Navigator widget  –  thumbnail + scale slider + angle slider + reset button
// ─────────────────────────────────────────────────────────────────────────────
QWidget *MainWindow::buildNavigatorWidget()
{
    auto *w = new QWidget(this);
    auto *l = new QVBoxLayout(w);
    l->setContentsMargins(4, 4, 4, 4);
    l->setSpacing(4);

    // Live canvas thumbnail (local class — only needed here)
    class NavThumb : public QWidget {
    public:
        CanvasWidget *canvas = nullptr;
        explicit NavThumb(QWidget *p) : QWidget(p) {
            setMinimumHeight(100);
            setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        }
    protected:
        void paintEvent(QPaintEvent *) override {
            QPainter p(this);
            p.fillRect(rect(), QColor("#111"));
            if (!canvas) return;
            const QImage &img = canvas->compositedImage();
            if (img.isNull()) return;
            const QSizeF scaled = img.size().scaled(size(), Qt::KeepAspectRatio);
            const QRectF dst(
                (width()  - scaled.width())  * 0.5,
                (height() - scaled.height()) * 0.5,
                scaled.width(), scaled.height());
            p.setRenderHint(QPainter::SmoothPixmapTransform);
            p.drawImage(dst, img);
            p.setPen(QPen(QColor("#4a6fa5"), 1));
            p.setBrush(Qt::NoBrush);
            p.drawRect(dst.adjusted(0, 0, -1, -1));
        }
    };

    auto *thumb = new NavThumb(w);
    thumb->canvas = m_canvas;
    connect(m_canvas,     &CanvasWidget::zoomChanged,          thumb, [thumb](float)  { thumb->update(); });
    connect(m_layerStack, &LayerStack::layersChanged,          thumb, [thumb]()       { thumb->update(); });
    connect(m_layerStack, &LayerStack::layerPropertiesChanged, thumb, [thumb]()       { thumb->update(); });
    connect(m_canvas,     &CanvasWidget::canvasUpdated,        thumb, [thumb]()       { thumb->update(); });
    l->addWidget(thumb, 1);

    // Scale row
    auto *scaleRow    = new QHBoxLayout;
    auto *scaleLbl    = new QLabel(tr("Scale"), w);
    scaleLbl->setFixedWidth(38);
    auto *scaleSlider = new QSlider(Qt::Horizontal, w);
    scaleSlider->setRange(5, 3200);
    scaleSlider->setValue(100);
    connect(scaleSlider, &QSlider::valueChanged, this, &MainWindow::onQuickZoomChanged);
    connect(m_canvas, &CanvasWidget::zoomChanged, w, [scaleSlider](float z) {
        const QSignalBlocker b(scaleSlider);
        scaleSlider->setValue(static_cast<int>(z * 100.0f));
    });
    scaleRow->addWidget(scaleLbl);
    scaleRow->addWidget(scaleSlider, 1);
    l->addLayout(scaleRow);

    // Angle row
    auto *angleRow    = new QHBoxLayout;
    auto *angleLbl    = new QLabel(tr("Angle"), w);
    angleLbl->setFixedWidth(38);
    auto *angleSlider = new QSlider(Qt::Horizontal, w);
    angleSlider->setRange(-180, 180);
    angleSlider->setValue(0);
    connect(angleSlider, &QSlider::valueChanged, m_canvas, [this](int deg) {
        m_canvas->setRotation(static_cast<float>(deg));
    });
    connect(m_canvas, &CanvasWidget::rotationChanged, w, [angleSlider](float deg) {
        const QSignalBlocker b(angleSlider);
        angleSlider->setValue(static_cast<int>(deg));
    });
    auto *resetAngleBtn = new QToolButton(w);
    resetAngleBtn->setText("↺");
    resetAngleBtn->setFixedSize(22, 22);
    resetAngleBtn->setToolTip(tr("Reset rotation"));
    connect(resetAngleBtn, &QToolButton::clicked, w, [angleSlider]() {
        angleSlider->setValue(0);
    });
    angleRow->addWidget(angleLbl);
    angleRow->addWidget(angleSlider, 1);
    angleRow->addWidget(resetAngleBtn);
    l->addLayout(angleRow);

    return w;
}

// ─────────────────────────────────────────────────────────────────────────────
// Color widget  –  ColorWheelWidget + primary/secondary swatches
// ─────────────────────────────────────────────────────────────────────────────
QWidget *MainWindow::buildColorWidget()
{
    m_colorPanel = new ColorPanelWidget(this);
    auto *colorPanel = m_colorPanel;
    colorPanel->setMinimumWidth(200);

    colorPanel->setBrushEngine(m_brushEngine);

    connect(colorPanel, &ColorPanelWidget::colorChanged,
            m_brushEngine, &BrushEngine::setColor);

    return colorPanel;
}
 
// ─────────────────────────────────────────────────────────────────────────────
// Brush widget  –  brush type tabs + blend row + preset list + sliders
// ─────────────────────────────────────────────────────────────────────────────
QWidget *MainWindow::buildBrushWidget()
{
    auto *brushBody = m_toolbar->takeBrushBody();
    if (!brushBody) return new QWidget(this);

    brushBody->setParent(this);

    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setWidget(brushBody);
    return scroll;
}
