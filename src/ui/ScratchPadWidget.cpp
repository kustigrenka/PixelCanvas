#include "ScratchPadWidget.h"
#include "BrushEngine.h"
#include "Stroke.h"

#include <QPainter>
#include <QTabletEvent>
#include <QMouseEvent>
#include <cmath>

ScratchPadWidget::ScratchPadWidget(QWidget *parent)
    : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMinimumSize(80, 60);
    setMouseTracking(true);
    setAttribute(Qt::WA_AcceptTouchEvents, false);
    setCursor(Qt::CrossCursor);
}

ScratchPadWidget::~ScratchPadWidget() = default;

void ScratchPadWidget::setBrushEngine(BrushEngine *engine) { m_engine = engine; }
void ScratchPadWidget::setColor(const QColor &c)           { m_color  = c;      }

void ScratchPadWidget::setImage(const QImage &img)
{
    if (img.isNull()) return;
    ensureCanvas();
    QPainter p(&m_canvas);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    p.drawImage(rect(), img);
    update();
}

void ScratchPadWidget::clear()
{
    ensureCanvas();
    m_canvas.fill(Qt::white);
    update();
}

// ─────────────────────────────────────────────────────────────────────────────
// FIX 3: ensureCanvas — only reallocate when size actually changed.
//
// The accordion animation drives the container from maxH=0 to maxH=naturalH,
// so the scratchpad receives many resize events while being revealed.
// We keep m_canvasSize separate from the widget size: if the new size is
// smaller we just paint a cropped view; if it grows we blit the old content
// into a fresh background so no strokes are lost.
// ─────────────────────────────────────────────────────────────────────────────
void ScratchPadWidget::ensureCanvas()
{
    const QSize needed = size().expandedTo(QSize(1, 1));

    if (m_canvas.isNull()) {
        // First allocation
        m_canvas     = QImage(needed, QImage::Format_ARGB32_Premultiplied);
        m_canvas.fill(QColor(55, 55, 55));
        m_canvasSize = needed;
        if (m_engine) m_engine->setActiveLayer(&m_canvas);
        return;
    }

    if (needed == m_canvasSize) return;  // nothing to do

    if (needed.width()  <= m_canvasSize.width() &&
        needed.height() <= m_canvasSize.height())
    {
        // Widget shrank (e.g. closing animation) — keep the large canvas,
        // just repaint a subset.  Don't reallocate.
        return;
    }

    // Widget grew — create a new canvas and blit old content into it.
    QImage fresh(needed, QImage::Format_ARGB32_Premultiplied);
    fresh.fill(QColor(55, 55, 55));
    {
        QPainter p(&fresh);
        p.drawImage(0, 0, m_canvas);   // blit at (0,0), new space is gray
    }
    m_canvas     = std::move(fresh);
    m_canvasSize = needed;
    if (m_engine) m_engine->setActiveLayer(&m_canvas);
}

// ─────────────────────────────────────────────────────────────────────────────
// Paint
// ─────────────────────────────────────────────────────────────────────────────
void ScratchPadWidget::paintEvent(QPaintEvent *)
{
    ensureCanvas();

    QPainter p(this);
    // Draw only the visible portion of the canvas (widget may be smaller than canvas)
    p.drawImage(0, 0, m_canvas, 0, 0, width(), height());

    // Border
    p.setPen(QPen(QColor(25, 25, 25), 1));
    p.setBrush(Qt::NoBrush);
    p.drawRect(rect().adjusted(0, 0, -1, -1));

    // Brush cursor
    if (m_cursorOnPad && m_engine) {
        const float r = m_engine->settings().size * 0.5f;
        if (!m_altHeld) {
            p.setPen(QPen(Qt::white, 1.2));
            p.setBrush(Qt::NoBrush);
            p.drawEllipse(m_cursorPos, r, r);
            p.setPen(QPen(Qt::black, 0.7));
            p.drawEllipse(m_cursorPos, r - 1.0f, r - 1.0f);
        } else {
            p.setPen(QPen(Qt::white, 1.5));
            p.drawLine(m_cursorPos + QPointF(-6,0), m_cursorPos + QPointF(6,0));
            p.drawLine(m_cursorPos + QPointF(0,-6), m_cursorPos + QPointF(0,6));
        }
    }
}

void ScratchPadWidget::resizeEvent(QResizeEvent *)
{
    // FIX 3: Do NOT wipe m_canvas here.  ensureCanvas() in paintEvent/pointerBegin
    // handles growing the canvas while preserving content.
    // Old code: m_canvas = QImage();  ← this was the bug, wiping on every animation frame.
}

// ─────────────────────────────────────────────────────────────────────────────
// Stroke helpers
// ─────────────────────────────────────────────────────────────────────────────
void ScratchPadWidget::pointerBegin(const QPointF &pos, float pressure)
{
    ensureCanvas();
    if (!m_engine) return;

    if (m_altHeld) { pickColor(pos); return; }

    m_savedLayer = m_engine->activeLayer();
    m_engine->setActiveLayer(&m_canvas);
    m_engine->setColor(m_color);

    m_drawing = true;
    m_engine->beginStroke();

    StrokeSample s;
    s.pos = pos; s.pressure = pressure; s.tiltX = s.tiltY = s.rotation = 0.f;
    const QRect dirty = m_engine->addSample(s);
    if (!dirty.isEmpty()) update(dirty.adjusted(-2,-2,2,2));
}

void ScratchPadWidget::pointerUpdate(const QPointF &pos, float pressure)
{
    if (m_altHeld && m_drawing) { pickColor(pos); return; }
    if (!m_drawing || !m_engine) return;

    StrokeSample s;
    s.pos = pos; s.pressure = pressure; s.tiltX = s.tiltY = s.rotation = 0.f;
    const QRect dirty = m_engine->addSample(s);
    if (!dirty.isEmpty()) update(dirty.adjusted(-2,-2,2,2));
}

void ScratchPadWidget::pointerEnd()
{
    if (!m_drawing || !m_engine) return;
    const QRect dirty = m_engine->endStroke();
    if (!dirty.isEmpty()) update(dirty.adjusted(-2,-2,2,2));
    m_engine->setActiveLayer(m_savedLayer);
    m_savedLayer = nullptr;
    m_drawing    = false;
}

void ScratchPadWidget::pickColor(const QPointF &pos)
{
    if (m_canvas.isNull()) return;
    const int x = std::clamp(static_cast<int>(pos.x()), 0, m_canvas.width()  - 1);
    const int y = std::clamp(static_cast<int>(pos.y()), 0, m_canvas.height() - 1);
    const QColor c = m_canvas.pixelColor(x, y);
    if (c.isValid()) emit colorPicked(c);
}

// ─────────────────────────────────────────────────────────────────────────────
// Tablet
// ─────────────────────────────────────────────────────────────────────────────
void ScratchPadWidget::tabletEvent(QTabletEvent *e)
{
    e->accept();
    m_cursorPos = e->position();
    const float pressure = static_cast<float>(e->pressure());

    switch (e->type()) {
    case QEvent::TabletPress:
        m_tabletInUse = true;
        m_altHeld = (e->modifiers() & Qt::AltModifier);
        pointerBegin(e->position(), pressure);
        break;
    case QEvent::TabletMove:
        m_altHeld = (e->modifiers() & Qt::AltModifier);
        pointerUpdate(e->position(), pressure);
        break;
    case QEvent::TabletRelease:
        pointerEnd();
        m_tabletInUse = false;
        break;
    default: break;
    }
    if (m_engine) {
        const float r = m_engine->settings().size * 0.5f + 4.f;
        update(QRectF(m_cursorPos - QPointF(r,r), QSizeF(r*2,r*2)).toAlignedRect());
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Mouse
// ─────────────────────────────────────────────────────────────────────────────
void ScratchPadWidget::mousePressEvent(QMouseEvent *e)
{
    if (m_tabletInUse) return;
    if (e->button() == Qt::RightButton) { clear(); return; }
    if (e->button() == Qt::LeftButton) {
        m_altHeld   = (e->modifiers() & Qt::AltModifier);
        m_cursorPos = e->position();
        pointerBegin(e->position(), 1.0f);
    }
}

void ScratchPadWidget::mouseMoveEvent(QMouseEvent *e)
{
    if (m_tabletInUse) return;
    m_altHeld = (e->modifiers() & Qt::AltModifier);
    const QPointF prev = m_cursorPos;
    m_cursorPos = e->position();
    if (m_engine) {
        const float r = m_engine->settings().size * 0.5f + 4.f;
        update(QRectF(prev        - QPointF(r,r), QSizeF(r*2,r*2)).toAlignedRect());
        update(QRectF(m_cursorPos - QPointF(r,r), QSizeF(r*2,r*2)).toAlignedRect());
    }
    pointerUpdate(e->position(), 1.0f);
}

void ScratchPadWidget::mouseReleaseEvent(QMouseEvent *e)
{
    if (m_tabletInUse) return;
    if (e->button() == Qt::LeftButton) { pointerEnd(); m_altHeld = false; }
}

void ScratchPadWidget::enterEvent(QEnterEvent *e)
{
    m_cursorOnPad = true;
    m_cursorPos   = e->position();
    update();
    QWidget::enterEvent(e);
}

void ScratchPadWidget::leaveEvent(QEvent *e)
{
    m_cursorOnPad = false;
    update();
    QWidget::leaveEvent(e);
}
