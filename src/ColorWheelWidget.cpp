#include "ColorWheelWidget.h"

#include <QPainter>
#include <QMouseEvent>
#include <QtMath>
#include <cmath>

// ─────────────────────────────────────────────────────────────────────────────
// Construction
// ─────────────────────────────────────────────────────────────────────────────
ColorWheelWidget::ColorWheelWidget(QWidget *parent)
    : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMinimumSize(180, 180);
}
// ─────────────────────────────────────────────────────────────────────────────
// Public API
// ─────────────────────────────────────────────────────────────────────────────
void ColorWheelWidget::setColor(const QColor &c)
{
    if (m_color == c) return;
    m_color = c;
    m_hue = (c.hsvHueF() < 0.0f) ? 0.0f : static_cast<float>(c.hsvHueF());
    m_sat = static_cast<float>(c.hsvSaturationF());
    m_val = static_cast<float>(c.valueF());
    m_squareDirty = true;
    update();
}

// ─────────────────────────────────────────────────────────────────────────────
// Geometry helpers
// ─────────────────────────────────────────────────────────────────────────────
QPointF ColorWheelWidget::center() const
{
    return { width() / 2.0, height() / 2.0 };
}

float ColorWheelWidget::outerRadius() const
{
    return qMin(width(), height()) / 2.0f - 2.0f;
}

float ColorWheelWidget::innerRadius() const
{
    return outerRadius() * 0.72f;   // ring is 28% of radius wide
}

QRectF ColorWheelWidget::squareRect() const
{
    // Inscribed square inside the inner circle
    const float r  = innerRadius() * 0.95f;
    const float s  = r * static_cast<float>(M_SQRT2) * 0.97f;
    const QPointF c = center();
    return QRectF(c.x() - s / 2.0, c.y() - s / 2.0, s, s);
}

// ─────────────────────────────────────────────────────────────────────────────
// Cache rebuilders
// ─────────────────────────────────────────────────────────────────────────────
void ColorWheelWidget::rebuildCache()
{
    const int W = width();
    const int H = height();
    const QPointF c = center();
    const float outer = outerRadius();
    const float inner = innerRadius();

    // ── Wheel image ─────────────────────────────────────────────────────────
    if (m_cacheDirty) {
        m_wheelImg = QImage(W, H, QImage::Format_ARGB32_Premultiplied);
        m_wheelImg.fill(Qt::transparent);

        for (int y = 0; y < H; ++y) {
            QRgb *line = reinterpret_cast<QRgb *>(m_wheelImg.scanLine(y));
            for (int x = 0; x < W; ++x) {
                const float dx = x - static_cast<float>(c.x());
                const float dy = y - static_cast<float>(c.y());
                const float dist = std::sqrt(dx * dx + dy * dy);
                if (dist < inner || dist > outer) continue;
                float angle = std::atan2(dy, dx);          // -π … +π
                if (angle < 0) angle += 2.0f * static_cast<float>(M_PI);
                const float hue = angle / (2.0f * static_cast<float>(M_PI));
                // Anti-alias at edges
                float alpha = 1.0f;
                if (dist < inner + 1.5f) alpha = (dist - inner) / 1.5f;
                if (dist > outer - 1.5f) alpha = (outer - dist) / 1.5f;
                alpha = std::clamp(alpha, 0.0f, 1.0f);
                const QColor col = QColor::fromHsvF(
                    static_cast<double>(hue), 1.0, 1.0,
                    static_cast<double>(alpha));
                line[x] = col.rgba();
            }
        }
        m_cacheDirty = false;
    }

    // ── SV square ───────────────────────────────────────────────────────────
    if (m_squareDirty) {
        const QRectF sq = squareRect();
        const int sw = static_cast<int>(sq.width());
        const int sh = static_cast<int>(sq.height());
        if (sw > 0 && sh > 0) {
            m_squareImg = QImage(sw, sh, QImage::Format_ARGB32_Premultiplied);
            for (int sy = 0; sy < sh; ++sy) {
                QRgb *line = reinterpret_cast<QRgb *>(m_squareImg.scanLine(sy));
                const float val = 1.0f - sy / static_cast<float>(sh - 1);
                for (int sx = 0; sx < sw; ++sx) {
                    const float sat = sx / static_cast<float>(sw - 1);
                    line[sx] = QColor::fromHsvF(
                        static_cast<double>(m_hue),
                        static_cast<double>(sat),
                        static_cast<double>(val)).rgb();
                }
            }
        }
        m_squareDirty = false;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Paint
// ─────────────────────────────────────────────────────────────────────────────
void ColorWheelWidget::paintEvent(QPaintEvent *)
{
    rebuildCache();

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);

    // ── Draw SV square ───────────────────────────────────────────────────────
    const QRectF sq = squareRect();
    p.drawImage(sq, m_squareImg);

    // ── Draw hue ring on top ─────────────────────────────────────────────────
    p.drawImage(0, 0, m_wheelImg);

    // ── Hue cursor on ring ───────────────────────────────────────────────────
    {
        const float angle  = m_hue * 2.0f * static_cast<float>(M_PI);
        const float midR   = (outerRadius() + innerRadius()) / 2.0f;
        const QPointF cc   = center();
        const QPointF hPos(cc.x() + midR * std::cos(angle),
                           cc.y() + midR * std::sin(angle));

        const float cursorR = (outerRadius() - innerRadius()) / 2.0f - 1.0f;
        p.setPen(QPen(Qt::white, 1.5f));
        p.setBrush(QColor::fromHsvF(static_cast<double>(m_hue), 1.0, 1.0));
        p.drawEllipse(hPos, cursorR, cursorR);
    }

    // ── SV cursor inside square ──────────────────────────────────────────────
    {
        const float cx = sq.left() + m_sat * static_cast<float>(sq.width());
        const float cy = sq.top()  + (1.0f - m_val) * static_cast<float>(sq.height());
        p.setPen(QPen(Qt::white, 1.5f));
        p.setBrush(m_color);
        p.drawEllipse(QPointF(cx, cy), 5.0, 5.0);
        p.setPen(QPen(Qt::black, 0.75f));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(QPointF(cx, cy), 5.5, 5.5);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Mouse events
// ─────────────────────────────────────────────────────────────────────────────
void ColorWheelWidget::mousePressEvent(QMouseEvent *e)
{
    if (e->button() != Qt::LeftButton) return;

    const QPointF pos = e->position();
    const QPointF c   = center();
    const float dx    = static_cast<float>(pos.x() - c.x());
    const float dy    = static_cast<float>(pos.y() - c.y());
    const float dist  = std::sqrt(dx * dx + dy * dy);

    if (dist >= innerRadius() && dist <= outerRadius()) {
        m_drag = DragZone::Ring;
        pickFromRing(pos);
    } else if (squareRect().contains(pos)) {
        m_drag = DragZone::Square;
        pickFromSquare(pos);
    }
}

void ColorWheelWidget::mouseMoveEvent(QMouseEvent *e)
{
    if (m_drag == DragZone::None) return;
    handleMouse(e->position());
}

void ColorWheelWidget::mouseReleaseEvent(QMouseEvent *)
{
    m_drag = DragZone::None;
}

void ColorWheelWidget::handleMouse(QPointF pos)
{
    if (m_drag == DragZone::Ring)   pickFromRing(pos);
    if (m_drag == DragZone::Square) pickFromSquare(pos);
}

void ColorWheelWidget::pickFromRing(QPointF pos)
{
    const QPointF c = center();
    float angle = std::atan2(
        static_cast<float>(pos.y() - c.y()),
        static_cast<float>(pos.x() - c.x()));
    if (angle < 0) angle += 2.0f * static_cast<float>(M_PI);
    m_hue = angle / (2.0f * static_cast<float>(M_PI));
    m_squareDirty = true;
    m_color = QColor::fromHsvF(
        static_cast<double>(m_hue),
        static_cast<double>(m_sat),
        static_cast<double>(m_val));
    update();
    emit colorChanged(m_color);
}

void ColorWheelWidget::pickFromSquare(QPointF pos)
{
    const QRectF sq = squareRect();
    m_sat = std::clamp(
        static_cast<float>((pos.x() - sq.left()) / sq.width()),  0.0f, 1.0f);
    m_val = std::clamp(
        1.0f - static_cast<float>((pos.y() - sq.top()) / sq.height()), 0.0f, 1.0f);
    m_color = QColor::fromHsvF(
        static_cast<double>(m_hue),
        static_cast<double>(m_sat),
        static_cast<double>(m_val));
    update();
    emit colorChanged(m_color);
}

// ─────────────────────────────────────────────────────────────────────────────
// Resize
// ─────────────────────────────────────────────────────────────────────────────
void ColorWheelWidget::resizeEvent(QResizeEvent *)
{
    m_cacheDirty  = true;
    m_squareDirty = true;
}