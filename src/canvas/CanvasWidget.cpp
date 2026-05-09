#include "CanvasWidget.h"

#include <QPainter>
#include <QTabletEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include "LayerStack.h"
#include "BrushEngine.h"
#include "UndoStack.h"
#include "Filter.h"
#include "Layer.h"

// ─────────────────────────────────────────────────────────────────────────────
CanvasWidget::CanvasWidget(LayerStack  *layerStack,
                           BrushEngine *brushEngine,
                           UndoStack   *undoStack,
                           QWidget     *parent)
    : QOpenGLWidget(parent)
    , m_layerStack(layerStack)
    , m_brushEngine(brushEngine)
    , m_undoStack(undoStack)
{
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_AcceptTouchEvents, false);
    setCursor(Qt::BlankCursor);
    setMouseTracking(true);
}

CanvasWidget::~CanvasWidget() = default;

// ─────────────────────────────────────────────────────────────────────────────
// QOpenGLWidget lifecycle
// ─────────────────────────────────────────────────────────────────────────────

void CanvasWidget::initializeGL()
{
    const QSize canvasSize(2048, 2048);
    m_composited = QImage(canvasSize, QImage::Format_ARGB32_Premultiplied);
    m_composited.fill(Qt::white);

    m_layerStack->init(canvasSize);

    if (Layer *l = m_layerStack->activeLayer())
        m_brushEngine->setActiveLayer(&l->pixels);

    resetView();
}

void CanvasWidget::resizeGL(int /*w*/, int /*h*/)
{
}

void CanvasWidget::paintGL()
{
    QPainter p(this);
    p.fillRect(rect(), QColor(0x6E, 0x6E, 0x6E));
    p.setRenderHint(QPainter::SmoothPixmapTransform, m_zoom < 1.0);

    const QPointF canvasCentre(
        m_composited.width()  * m_zoom * 0.5,
        m_composited.height() * m_zoom * 0.5);

    p.translate(m_offset + canvasCentre);
    if (m_flipH)             p.scale(-1.0, 1.0);
    if (m_rotation != 0.0f) p.rotate(static_cast<double>(m_rotation));
    p.scale(m_zoom, m_zoom);
    p.translate(-m_composited.width() * 0.5, -m_composited.height() * 0.5);

    p.drawImage(QPointF(0, 0), m_composited);

    p.setCompositionMode(QPainter::CompositionMode_SourceOver);
    p.setPen(QPen(QColor(0x30, 0x30, 0x30), 1.0 / m_zoom));
    p.setBrush(Qt::NoBrush);
    p.drawRect(QRectF(QPointF(0, 0), QSizeF(m_composited.size())));

    // ── Brush cursor circle (widget space) ───────────────────────────────────
    if (m_cursorOnCanvas && !m_spaceHeld && !m_altHeld)
    {
        p.resetTransform();
        // Use effectiveDiameter() so the circle matches what actually gets drawn
        const float brushDiameterWidget =
            m_brushEngine->settings().effectiveDiameter() * m_zoom;
        const float r = brushDiameterWidget * 0.5f;

        p.setPen(QPen(Qt::white, 1.5));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(m_cursorPos, r, r);
        p.setPen(QPen(Qt::black, 0.75));
        p.drawEllipse(m_cursorPos, r - 1.0f, r - 1.0f);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Coordinate transform
// ─────────────────────────────────────────────────────────────────────────────

QPointF CanvasWidget::widgetToCanvas(const QPointF &wp) const
{
    const QPointF canvasCentre(
        m_composited.width()  * m_zoom * 0.5,
        m_composited.height() * m_zoom * 0.5);
    QPointF pt = wp - (m_offset + canvasCentre);

    if (m_flipH) pt.setX(-pt.x());

    if (m_rotation != 0.0f) {
        const double rad = -static_cast<double>(m_rotation) * M_PI / 180.0;
        const double c   = std::cos(rad);
        const double s   = std::sin(rad);
        pt = QPointF(c * pt.x() - s * pt.y(),
                     s * pt.x() + c * pt.y());
    }

    pt /= m_zoom;
    pt += QPointF(m_composited.width() * 0.5, m_composited.height() * 0.5);
    return pt;
}

// ─────────────────────────────────────────────────────────────────────────────
// Viewport control
// ─────────────────────────────────────────────────────────────────────────────

void CanvasWidget::setZoom(float zoom)
{
    m_zoom = std::clamp(zoom, 0.05f, 32.0f);
    update();
    emit zoomChanged(m_zoom);
}

void CanvasWidget::setRotation(float degrees)
{
    float r = std::fmod(degrees, 360.0f);
    if (r >  180.0f) r -= 360.0f;
    if (r < -180.0f) r += 360.0f;
    if (m_rotation == r) return;
    m_rotation = r;
    update();
    emit rotationChanged(m_rotation);
}

void CanvasWidget::setFlipH(bool flip)
{
    if (m_flipH == flip) return;
    m_flipH = flip;
    update();
    emit flipHChanged(m_flipH);
}

void CanvasWidget::resetView()
{
    const QSizeF cs = m_composited.size();
    const float margin = 40.0f;

    const float fitZoom = std::min(
        (width()  - margin * 2) / cs.width(),
        (height() - margin * 2) / cs.height()
    );
    m_zoom = std::clamp(fitZoom, 0.05f, 1.0f);

    m_offset = QPointF(
        (width()  - cs.width()  * m_zoom) * 0.5f,
        (height() - cs.height() * m_zoom) * 0.5f
    );

    update();
    emit zoomChanged(m_zoom);
}

// ─────────────────────────────────────────────────────────────────────────────
// Dirty-rect recomposite
// ─────────────────────────────────────────────────────────────────────────────

void CanvasWidget::recompositeRect(const QRect &dirty)
{
    if (dirty.isEmpty()) return;
    m_layerStack->recompositeRect(m_composited, dirty);

    // Expand slightly to cover antialiased edges, then schedule a repaint
    // of only the affected widget region.
    const QRectF widgetDirty(
        m_offset.x() + dirty.x() * m_zoom,
        m_offset.y() + dirty.y() * m_zoom,
        dirty.width()  * m_zoom,
        dirty.height() * m_zoom);
    update(widgetDirty.toAlignedRect().adjusted(-2, -2, 2, 2));
    emit canvasUpdated();
}

// ─────────────────────────────────────────────────────────────────────────────
// Pointer logic  (shared by tablet + mouse paths)
// ─────────────────────────────────────────────────────────────────────────────

void CanvasWidget::pointerBegin(const QPointF &widgetPos, float pressure,
                                float tiltX, float tiltY, float rotation)
{
    if (m_spaceHeld || m_middlePanning)
    {
        m_panning        = true;
        m_panStartWidget = widgetPos;
        m_panStartOffset = m_offset;
        return;
    }

    // Skip drawing for non-painting tools (Bucket etc.) — handled separately
    const TipType tt = m_brushEngine->settings().tipType;
    if (tt == TipType::Bucket || tt == TipType::Gradient)
        return;

    if (m_undoStack)
        m_undoStack->pushSnapshot(m_layerStack->activeIndex(),
                                  m_layerStack->activeLayer()->pixels);

    if (Layer *l = m_layerStack->activeLayer())
        m_brushEngine->setActiveLayer(&l->pixels);

    m_drawing = true;
    m_brushEngine->beginStroke();

    StrokeSample s;
    s.pos      = widgetToCanvas(widgetPos);
    s.pressure = pressure;
    s.tiltX    = tiltX;
    s.tiltY    = tiltY;
    s.rotation = rotation;

    const QRect dirty = m_brushEngine->addSample(s);
    if (!dirty.isEmpty())
        recompositeRect(dirty);

    emit cursorMoved(s.pos, pressure);
}

void CanvasWidget::pointerUpdate(const QPointF &widgetPos, float pressure,
                                 float tiltX, float tiltY, float rotation)
{
    if (m_panning)
    {
        m_offset = m_panStartOffset + (widgetPos - m_panStartWidget);
        update();
        return;
    }

    if (!m_drawing) return;

    StrokeSample s;
    s.pos      = widgetToCanvas(widgetPos);
    s.pressure = pressure;
    s.tiltX    = tiltX;
    s.tiltY    = tiltY;
    s.rotation = rotation;

    const QRect dirty = m_brushEngine->addSample(s);
    // Only recomposite+repaint if the brush actually stamped a dab
    if (!dirty.isEmpty())
        recompositeRect(dirty);

    emit cursorMoved(s.pos, pressure);
}

void CanvasWidget::pointerEnd()
{
    if (m_panning) { m_panning = false; return; }
    if (m_drawing)
    {
        const QRect dirty = m_brushEngine->endStroke();
        if (!dirty.isEmpty())
            recompositeRect(dirty);
        m_drawing = false;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Tablet events
// ─────────────────────────────────────────────────────────────────────────────

void CanvasWidget::tabletEvent(QTabletEvent *e)
{
    e->accept();

    if (m_altHeld) return;  // eyedropper — TODO Ph.8

    const float pressure = static_cast<float>(e->pressure());
    const float tiltX    = static_cast<float>(e->xTilt());
    const float tiltY    = static_cast<float>(e->yTilt());
    const float rotation = static_cast<float>(e->rotation());

    switch (e->type())
    {
    case QEvent::TabletPress:
        m_tabletInUse = true;
        m_cursorPos   = e->position();
        pointerBegin(e->position(), pressure, tiltX, tiltY, rotation);
        break;
    case QEvent::TabletMove:
        m_cursorPos = e->position();
        pointerUpdate(e->position(), pressure, tiltX, tiltY, rotation);
        // Always repaint cursor circle even if no dab was stamped
        update();
        break;
    case QEvent::TabletRelease:
        pointerEnd();
        m_tabletInUse = false;
        break;
    default:
        break;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Mouse events
// ─────────────────────────────────────────────────────────────────────────────

void CanvasWidget::mousePressEvent(QMouseEvent *e)
{
    if (m_tabletInUse) return;
    if (e->button() == Qt::MiddleButton)
    {
        m_middlePanning = true;
        pointerBegin(e->position(), 1.0f, 0.f, 0.f, 0.f);
        return;
    }
    if (e->button() == Qt::LeftButton)
        pointerBegin(e->position(), 1.0f, 0.f, 0.f, 0.f);
}

void CanvasWidget::mouseMoveEvent(QMouseEvent *e)
{
    if (m_tabletInUse) return;
    m_cursorPos = e->position();

    // pointerUpdate calls recompositeRect which calls update() for dirty rects.
    // If not drawing we still need a repaint for the cursor circle — one update()
    // covers both cases because Qt coalesces multiple update() calls per frame.
    pointerUpdate(e->position(), 1.0f, 0.f, 0.f, 0.f);
    if (!m_drawing)
        update();
}

void CanvasWidget::mouseReleaseEvent(QMouseEvent *e)
{
    if (m_tabletInUse) return;
    if (e->button() == Qt::MiddleButton)
        m_middlePanning = false;
    pointerEnd();
}

// ─────────────────────────────────────────────────────────────────────────────
// Enter / Leave
// ─────────────────────────────────────────────────────────────────────────────

void CanvasWidget::enterEvent(QEnterEvent *e)
{
    m_cursorOnCanvas = true;
    m_cursorPos      = e->position();
    update();
    QOpenGLWidget::enterEvent(e);
}

void CanvasWidget::leaveEvent(QEvent *e)
{
    m_cursorOnCanvas = false;
    update();
    QOpenGLWidget::leaveEvent(e);
}

// ─────────────────────────────────────────────────────────────────────────────
// Wheel → zoom
// ─────────────────────────────────────────────────────────────────────────────

void CanvasWidget::wheelEvent(QWheelEvent *e)
{
    if (e->modifiers() & Qt::ControlModifier)
    {
        e->accept();
        const float delta  = e->angleDelta().y() / 120.0f;
        const float factor = std::pow(1.15f, delta);
        const QPointF pivot = e->position();
        m_offset = pivot + (m_offset - pivot) * factor;
        setZoom(m_zoom * factor);
    }
    else
    {
        m_offset.setY(m_offset.y() + e->angleDelta().y() * 0.5);
        update();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Key events
// ─────────────────────────────────────────────────────────────────────────────

void CanvasWidget::keyPressEvent(QKeyEvent *e)
{
    switch (e->key())
    {
    case Qt::Key_Space:
        if (!e->isAutoRepeat())
        {
            m_spaceHeld = true;
            setCursor(Qt::OpenHandCursor);
        }
        break;
    case Qt::Key_Alt:
        m_altHeld = true;
        setCursor(Qt::CrossCursor);
        break;
    case Qt::Key_BracketLeft:
    {
        BrushSettings s = m_brushEngine->settings();
        s.size = std::max(1.0f, s.size - 2.0f);
        m_brushEngine->setSettings(s);
        update();
        break;
    }
    case Qt::Key_BracketRight:
    {
        BrushSettings s = m_brushEngine->settings();
        s.size = std::min(500.0f, s.size + 2.0f);
        m_brushEngine->setSettings(s);
        update();
        break;
    }
    default:
        QOpenGLWidget::keyPressEvent(e);
        break;
    }
}

void CanvasWidget::keyReleaseEvent(QKeyEvent *e)
{
    switch (e->key())
    {
    case Qt::Key_Space:
        if (!e->isAutoRepeat())
        {
            m_spaceHeld = false;
            setCursor(Qt::BlankCursor);
        }
        break;
    case Qt::Key_Alt:
        m_altHeld = false;
        setCursor(Qt::BlankCursor);
        break;
    default:
        QOpenGLWidget::keyReleaseEvent(e);
        break;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Filter application
// ─────────────────────────────────────────────────────────────────────────────

void CanvasWidget::applyFilter(Filter *filter)
{
    if (!filter) return;
    if (m_undoStack)
        m_undoStack->pushSnapshot(m_layerStack->activeIndex(),
                                  m_layerStack->activeLayer()->pixels);
    m_layerStack->applyFilterToActive(filter);
    recompositeRect(m_composited.rect());
}

void CanvasWidget::forceRecomposite()
{
    recompositeRect(m_composited.rect());
}

void CanvasWidget::reinitCanvas()
{
    const QSize newSize = m_layerStack->canvasSize();
    m_composited = QImage(newSize, QImage::Format_ARGB32_Premultiplied);
    m_composited.fill(Qt::white);

    if (Layer *l = m_layerStack->activeLayer())
        m_brushEngine->setActiveLayer(&l->pixels);

    recompositeRect(m_composited.rect());
    resetView();
}
