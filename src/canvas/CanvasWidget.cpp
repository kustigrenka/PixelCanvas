#include "CanvasWidget.h"

#include <QPainter>
#include <QTabletEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QStack>
#include <QLinearGradient>
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

    // Batched recomposite: accumulate dirty rects during a stroke and flush
    // at most ~120 times per second instead of once per dab.
    m_repaintTimer = new QTimer(this);
    m_repaintTimer->setSingleShot(true);
    m_repaintTimer->setInterval(8);   // 8 ms ≈ 120 fps cap
    connect(m_repaintTimer, &QTimer::timeout, this, &CanvasWidget::flushPendingDirty);
}

CanvasWidget::~CanvasWidget() = default;

// ─────────────────────────────────────────────────────────────────────────────
// QOpenGLWidget lifecycle
// ─────────────────────────────────────────────────────────────────────────────

void CanvasWidget::initializeGL() {}

void CanvasWidget::resizeGL(int /*w*/, int /*h*/) {}

void CanvasWidget::paintGL()
{
    QPainter p(this);
    p.fillRect(rect(), QColor(0x6E, 0x6E, 0x6E));

    if (m_composited.isNull()) return;

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

    // Shift straight-line preview — drawn in canvas space, before border
    if (m_shiftLineActive && m_shiftHeld && m_cursorOnCanvas)
    {
        p.setPen(QPen(QColor(100, 160, 255, 160), 1.0 / m_zoom, Qt::DashLine));
        p.drawLine(m_shiftLineStart, widgetToCanvas(m_cursorPos));
    }

    p.setCompositionMode(QPainter::CompositionMode_SourceOver);
    p.setPen(QPen(QColor(0x30, 0x30, 0x30), 1.0 / m_zoom));
    p.setBrush(Qt::NoBrush);
    p.drawRect(QRectF(QPointF(0, 0), QSizeF(m_composited.size())));

    // Brush cursor circle (widget space)
    if (m_cursorOnCanvas && !m_spaceHeld && !m_altHeld)
    {
        p.resetTransform();
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
// Viewport
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
        (height() - margin * 2) / cs.height());
    m_zoom = std::clamp(fitZoom, 0.05f, 1.0f);
    m_offset = QPointF(
        (width()  - cs.width()  * m_zoom) * 0.5f,
        (height() - cs.height() * m_zoom) * 0.5f);
    update();
    emit zoomChanged(m_zoom);
}

// ─────────────────────────────────────────────────────────────────────────────
// Recomposite — always suspend/resume stroke painter so LayerStack can open
// its own painter on the same QImage without Qt complaining.
// ─────────────────────────────────────────────────────────────────────────────

void CanvasWidget::recompositeRect(const QRect &dirty)
{
    if (dirty.isEmpty()) return;

    if (m_drawing)
        m_brushEngine->suspendStrokePainter();

    m_layerStack->recompositeRect(m_composited, dirty);

    if (m_drawing && m_brushEngine->hasScratch())
    {
        const QImage *scratch = m_brushEngine->strokeScratch();
        if (scratch && !scratch->isNull())
        {
            const BrushSettings &bs = m_brushEngine->settings();
            const bool isEraser = bs.tipType == TipType::Eraser ||
                                  bs.tipType == TipType::SelEraser ||
                                  bs.blendMode == BrushBlendMode::Erase;

            if (isEraser)
            {
                // Apply erase to a temp copy of the active layer pixels,
                // then recomposite that region so the white background shows through.
                if (Layer *layer = m_layerStack->activeLayer())
                {
                    QImage tempLayer = layer->pixels.copy(dirty);
                    QPainter tp(&tempLayer);
                    tp.setCompositionMode(QPainter::CompositionMode_DestinationOut);
                    tp.drawImage(QRect(0, 0, dirty.width(), dirty.height()),
                                 *scratch, dirty);
                    tp.end();

                    QPainter sp(&m_composited);
                    sp.setCompositionMode(QPainter::CompositionMode_Source);
                    sp.fillRect(dirty, Qt::white);
                    sp.setCompositionMode(QPainter::CompositionMode_SourceOver);
                    sp.drawImage(dirty.topLeft(), tempLayer);
                }
            }
            else
            {
                QPainter sp(&m_composited);
                sp.setCompositionMode(QPainter::CompositionMode_SourceOver);
                sp.setOpacity(static_cast<double>(bs.opacity));
                sp.drawImage(dirty, *scratch, dirty);
            }
        }
    }

    if (m_drawing)
        m_brushEngine->resumeStrokePainter();

    const QRectF widgetDirty(
        m_offset.x() + dirty.x() * m_zoom,
        m_offset.y() + dirty.y() * m_zoom,
        dirty.width()  * m_zoom,
        dirty.height() * m_zoom);
    update(widgetDirty.toAlignedRect().adjusted(-2, -2, 2, 2));
    emit canvasUpdated();
}

// Called by m_repaintTimer — flushes accumulated dirty rect from stroke dabs.
void CanvasWidget::flushPendingDirty()
{
    if (!m_pendingDirty.isEmpty())
    {
        recompositeRect(m_pendingDirty);
        m_pendingDirty = {};
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Bucket fill
// ─────────────────────────────────────────────────────────────────────────────

void CanvasWidget::doBucketFill(const QPointF &canvasPos)
{
    Layer *layer = m_layerStack->activeLayer();
    if (!layer) return;

    QImage &img = layer->pixels;
    const int W  = img.width();
    const int H  = img.height();
    const int x0 = static_cast<int>(canvasPos.x());
    const int y0 = static_cast<int>(canvasPos.y());
    if (x0 < 0 || y0 < 0 || x0 >= W || y0 >= H) return;

    // Read target color directly from scanline (fast, no lock per call)
    const QRgb target = reinterpret_cast<const QRgb*>(img.constScanLine(y0))[x0];
    const QRgb fill   = qPremultiply(m_brushEngine->color().rgba());
    if (target == fill) return;

    // Scanline flood fill — processes whole horizontal spans at once,
    // far fewer stack operations than pixel-by-pixel.
    QStack<QPoint> stack;
    stack.push({x0, y0});

    while (!stack.isEmpty())
    {
        const QPoint pt = stack.pop();
        int y = pt.y();
        if (y < 0 || y >= H) continue;

        QRgb *row = reinterpret_cast<QRgb*>(img.scanLine(y));

        // Walk left from seed
        int left = pt.x();
        while (left > 0 && row[left - 1] == target) --left;

        // Walk right from seed
        int right = pt.x();
        while (right < W - 1 && row[right + 1] == target) ++right;

        // Fill the span
        for (int x = left; x <= right; ++x)
            row[x] = fill;

        // Check spans above and below
        auto checkRow = [&](int ny) {
            if (ny < 0 || ny >= H) return;
            const QRgb *nrow = reinterpret_cast<const QRgb*>(img.constScanLine(ny));
            bool inSpan = false;
            for (int x = left; x <= right; ++x) {
                if (nrow[x] == target && !inSpan) {
                    stack.push({x, ny});
                    inSpan = true;
                } else if (nrow[x] != target) {
                    inSpan = false;
                }
            }
        };
        checkRow(y - 1);
        checkRow(y + 1);
    }
    recompositeRect(img.rect());
}

// ─────────────────────────────────────────────────────────────────────────────
// Pointer logic
// ─────────────────────────────────────────────────────────────────────────────

void CanvasWidget::pointerBegin(const QPointF &widgetPos, float pressure,
                                float tiltX, float tiltY, float rotation)
{
    if (m_composited.isNull()) return;

    if (m_spaceHeld || m_middlePanning)
    {
        m_panning        = true;
        m_panStartWidget = widgetPos;
        m_panStartOffset = m_offset;
        return;
    }

    if (m_shiftHeld) {
        m_shiftLineStart  = widgetToCanvas(widgetPos);
        m_shiftLineActive = true;
    }

    const TipType tt = m_brushEngine->settings().tipType;

    if (tt == TipType::Bucket)
    {
        if (m_undoStack)
            m_undoStack->pushSnapshot(m_layerStack->activeIndex(),
                                      m_layerStack->activeLayer()->pixels);
        doBucketFill(widgetToCanvas(widgetPos));
        return;
    }

    if (tt == TipType::Gradient)
    {
        m_gradientStart = widgetToCanvas(widgetPos);
        m_drawing = true;
        return;
    }

    if (tt == TipType::SelPen || tt == TipType::SelEraser)
    {
        if (!m_hasSelection || m_selectionMask.size() != m_composited.size())
        {
            m_selectionMask = QImage(m_composited.size(), QImage::Format_Grayscale8);
            m_selectionMask.fill(0);
            m_hasSelection = true;
        }
        m_brushEngine->setActiveLayer(&m_selectionMask);
        m_drawing = true;
        m_brushEngine->beginStroke();
        if (m_recordingEnabled)
            m_recorder.recordBegin(m_brushEngine->settings(),
                                   m_brushEngine->color(),
                                   m_layerStack->activeIndex());
        StrokeSample s;
        s.pos = widgetToCanvas(widgetPos); s.pressure = pressure;
        s.tiltX = tiltX; s.tiltY = tiltY; s.rotation = rotation;
        const QRect dirty = m_brushEngine->addSample(s);
        if (m_recordingEnabled) m_recorder.recordSample(s);
        if (!dirty.isEmpty()) {
            m_pendingDirty = m_pendingDirty.isEmpty() ? dirty : m_pendingDirty.united(dirty);
            if (!m_repaintTimer->isActive()) m_repaintTimer->start();
        }
        emit cursorMoved(s.pos, pressure);
        return;
    }

    // Normal brush path
    if (m_undoStack)
        m_undoStack->pushSnapshot(m_layerStack->activeIndex(),
                                  m_layerStack->activeLayer()->pixels);
    m_dirty = true;

    if (Layer *l = m_layerStack->activeLayer())
        m_brushEngine->setActiveLayer(&l->pixels);

    m_drawing = true;
    m_brushEngine->beginStroke();
    if (m_recordingEnabled)
        m_recorder.recordBegin(m_brushEngine->settings(),
                               m_brushEngine->color(),
                               m_layerStack->activeIndex());

    StrokeSample s;
    s.pos = widgetToCanvas(widgetPos); s.pressure = pressure;
    s.tiltX = tiltX; s.tiltY = tiltY; s.rotation = rotation;

    const QRect dirty = m_brushEngine->addSample(s);
    if (m_recordingEnabled) m_recorder.recordSample(s);
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
    s.pos = widgetToCanvas(widgetPos); s.pressure = pressure;
    s.tiltX = tiltX; s.tiltY = tiltY; s.rotation = rotation;

    const QRect dirty = m_brushEngine->addSample(s);
    if (m_recordingEnabled) m_recorder.recordSample(s);
    if (!dirty.isEmpty())
    {
        m_pendingDirty = m_pendingDirty.isEmpty() ? dirty : m_pendingDirty.united(dirty);
        if (!m_repaintTimer->isActive())
            m_repaintTimer->start();
    }

    emit cursorMoved(s.pos, pressure);
}

void CanvasWidget::pointerEnd()
{
    if (m_panning) { m_panning = false; return; }
    if (m_drawing)
    {
        m_repaintTimer->stop();
        flushPendingDirty();

        const TipType tt = m_brushEngine->settings().tipType;

        if (tt == TipType::Gradient)
        {
            Layer *layer = m_layerStack->activeLayer();
            if (layer && m_undoStack)
                m_undoStack->pushSnapshot(m_layerStack->activeIndex(), layer->pixels);
            if (layer)
            {
                const QPointF end = widgetToCanvas(m_cursorPos);
                QPainter gp(&layer->pixels);
                QLinearGradient grad(m_gradientStart, end);
                grad.setColorAt(0, m_brushEngine->color());
                grad.setColorAt(1, Qt::transparent);
                gp.fillRect(layer->pixels.rect(), grad);
                gp.end();
                recompositeRect(layer->pixels.rect());
            }
        }
        else
        {
            if (m_recordingEnabled) m_recorder.recordEnd();

            QRect dirty;
            if (m_shiftLineActive && m_shiftHeld)
            {
                // Cancel the in-progress stroke and replace it with a
                // clean straight line from the shift anchor to the cursor.
                m_brushEngine->endStroke();   // discard accumulated dabs

                const QPointF end = widgetToCanvas(m_cursorPos);
                StrokeSample s0, s1;
                s0.pos      = m_shiftLineStart; s0.pressure = 1.0f;
                s1.pos      = end;              s1.pressure = 1.0f;
                Stroke line; line << s0 << s1;
                dirty = m_brushEngine->renderStroke(line);
                m_shiftLineActive = false;
            }
            else
            {
                dirty = m_brushEngine->endStroke();
            }

            if (!dirty.isEmpty())
                recompositeRect(dirty);

            if (m_undoStack && m_layerStack->activeLayer())
                m_undoStack->pushSnapshot(m_layerStack->activeIndex(),
                                          m_layerStack->activeLayer()->pixels);
        }

        if (tt == TipType::SelPen || tt == TipType::SelEraser)
        {
            if (Layer *l = m_layerStack->activeLayer())
                m_brushEngine->setActiveLayer(&l->pixels);
        }

        m_drawing = false;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Tablet events
// ─────────────────────────────────────────────────────────────────────────────

void CanvasWidget::tabletEvent(QTabletEvent *e)
{
    e->accept();
    if (m_altHeld) return;

    const float pressure = static_cast<float>(e->pressure());
    const float tiltX    = static_cast<float>(e->xTilt());
    const float tiltY    = static_cast<float>(e->yTilt());
    const float rot      = static_cast<float>(e->rotation());

    switch (e->type())
    {
    case QEvent::TabletPress:
        m_tabletInUse = true;
        m_cursorPos   = e->position();
        if (e->button() == Qt::RightButton) {
            const QPointF cp = widgetToCanvas(e->position());
            const int x = static_cast<int>(cp.x());
            const int y = static_cast<int>(cp.y());
            if (m_composited.rect().contains(x, y)) {
                const QRgb px = reinterpret_cast<const QRgb *>(
                    m_composited.constScanLine(y))[x];
                m_brushEngine->setColor(QColor(qUnpremultiply(px)));
                emit colorPicked(QColor(qUnpremultiply(px)));
            }
        } else {
            pointerBegin(e->position(), pressure, tiltX, tiltY, rot);
        }
        break;
    case QEvent::TabletMove:
        m_cursorPos = e->position();
        pointerUpdate(e->position(), pressure, tiltX, tiltY, rot);
        update();
        break;
    case QEvent::TabletRelease:
        pointerEnd();
        m_tabletInUse = false;
        break;
    default: break;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Mouse events
// ─────────────────────────────────────────────────────────────────────────────

void CanvasWidget::mousePressEvent(QMouseEvent *e)
{
    if (m_tabletInUse) return;
    if (e->button() == Qt::MiddleButton) {
        m_middlePanning = true;
        pointerBegin(e->position(), 1.0f, 0.f, 0.f, 0.f);
        return;
    }
    if (e->button() == Qt::RightButton) {
        // Eyedropper: pick color from composited image
        const QPointF cp = widgetToCanvas(e->position());
        const int x = static_cast<int>(cp.x());
        const int y = static_cast<int>(cp.y());
        if (m_composited.rect().contains(x, y)) {
            const QRgb px = reinterpret_cast<const QRgb *>(
                m_composited.constScanLine(y))[x];
            const QColor picked = QColor(qUnpremultiply(px));
            m_brushEngine->setColor(picked);
            emit colorPicked(picked);
        }
        return;
    }
    if (e->button() == Qt::LeftButton)
        pointerBegin(e->position(), 1.0f, 0.f, 0.f, 0.f);
}

void CanvasWidget::mouseMoveEvent(QMouseEvent *e)
{
    if (m_tabletInUse) return;
    m_cursorPos = e->position();
    pointerUpdate(e->position(), 1.0f, 0.f, 0.f, 0.f);
    if (!m_drawing) update();
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
// Wheel
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
        if (!e->isAutoRepeat()) { m_spaceHeld = true; setCursor(Qt::OpenHandCursor); }
        break;
    case Qt::Key_Alt:
        m_altHeld = true; setCursor(Qt::CrossCursor);
        break;
    case Qt::Key_BracketLeft: {
        BrushSettings s = m_brushEngine->settings();
        s.size = std::max(1.0f, s.size - 2.0f);
        m_brushEngine->setSettings(s); update(); break; }
    case Qt::Key_BracketRight: {
        BrushSettings s = m_brushEngine->settings();
        s.size = std::min(500.0f, s.size + 2.0f);
        m_brushEngine->setSettings(s); update(); break; }
    case Qt::Key_Shift:
        if (!e->isAutoRepeat()) m_shiftHeld = true;
        break;
    default:
        QOpenGLWidget::keyPressEvent(e); break;
    }
}

void CanvasWidget::keyReleaseEvent(QKeyEvent *e)
{
    switch (e->key())
    {
    case Qt::Key_Space:
        if (!e->isAutoRepeat()) { m_spaceHeld = false; setCursor(Qt::BlankCursor); }
        break;
    case Qt::Key_Alt:
        m_altHeld = false; setCursor(Qt::BlankCursor);
        break;
    case Qt::Key_Shift:
    if (!e->isAutoRepeat()) {
        m_shiftHeld       = false;
        m_shiftLineActive = false;
        update();
    }
    break;
    default:
        QOpenGLWidget::keyReleaseEvent(e); break;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Filter / reinit
// ─────────────────────────────────────────────────────────────────────────────

void CanvasWidget::applyFilter(Filter *filter)
{
    if (!filter) return;
    if (m_undoStack)
        m_undoStack->pushSnapshot(m_layerStack->activeIndex(),
                                  m_layerStack->activeLayer()->pixels);
    m_layerStack->applyFilterToActive(filter);
    m_dirty = true;
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
    if (m_undoStack) m_undoStack->clear();
    m_dirty = false;
    recompositeRect(m_composited.rect());
    resetView();
}
