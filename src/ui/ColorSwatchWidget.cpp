#include "ColorSwatchWidget.h"

#include <QPainter>
#include <QMouseEvent>
#include <QToolTip>
#include <QMenu>
#include <QAction>
#include <QHelpEvent>
#include <algorithm>

ColorSwatchWidget::ColorSwatchWidget(QWidget *parent)
    : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    setMouseTracking(true);
    m_swatches.resize(kCount);
    recomputeCols();
}

// ─────────────────────────────────────────────────────────────────────────────
// Column recompute — called in resizeEvent and constructor
// ─────────────────────────────────────────────────────────────────────────────
void ColorSwatchWidget::recomputeCols()
{
    const int usable = std::max(kCellW + kPad, width() - 2 * kMargin + kPad);
    const int newCols = usable / (kCellW + kPad);
    if (newCols != m_cols) {
        m_cols = std::max(1, newCols);
        updateGeometry();   // tell accordion our height changed
        update();
    }
}

void ColorSwatchWidget::resizeEvent(QResizeEvent *)
{
    recomputeCols();
}

// ─────────────────────────────────────────────────────────────────────────────
// sizeHint — full grid height for current column count
// ─────────────────────────────────────────────────────────────────────────────
QSize ColorSwatchWidget::sizeHint() const
{
    return QSize(width() > 0 ? width() : 180, gridHeight());
}

// ─────────────────────────────────────────────────────────────────────────────
// Geometry
// ─────────────────────────────────────────────────────────────────────────────
QRect ColorSwatchWidget::swatchRect(int i) const
{
    const int c = i % m_cols;
    const int r = i / m_cols;
    return QRect(kMargin + c * (kCellW + kPad),
                 kMargin + r * (kCellH + kPad),
                 kCellW, kCellH);
}

int ColorSwatchWidget::swatchAt(QPoint pos) const
{
    for (int i = 0; i < kCount; ++i)
        if (swatchRect(i).contains(pos)) return i;
    return -1;
}

// ─────────────────────────────────────────────────────────────────────────────
// Paint
// ─────────────────────────────────────────────────────────────────────────────
void ColorSwatchWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);
    p.fillRect(rect(), QColor(42, 42, 42));

    for (int i = 0; i < kCount; ++i) {
        const QRect   r = swatchRect(i);
        const QColor &c = m_swatches.value(i);

        if (!c.isValid()) {
            constexpr int sq = 4;
            for (int cy = r.top(); cy < r.bottom(); cy += sq)
                for (int cx = r.left(); cx < r.right(); cx += sq)
                    p.fillRect(QRect(cx, cy,
                                     std::min(sq, r.right()  - cx),
                                     std::min(sq, r.bottom() - cy)),
                               ((cx/sq + cy/sq) % 2 == 0)
                               ? QColor(72,72,72) : QColor(52,52,52));
        } else {
            p.fillRect(r, c);
        }

        const bool active = c.isValid() && (c == m_color);
        p.setPen(QPen(active ? QColor(80,140,220) : QColor(28,28,28),
                      active ? 1.5 : 0.5));
        p.setBrush(Qt::NoBrush);
        p.drawRect(r);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Mouse
// ─────────────────────────────────────────────────────────────────────────────
void ColorSwatchWidget::mousePressEvent(QMouseEvent *e)
{
    const int idx = swatchAt(e->pos());
    if (idx < 0) return;

    if (e->button() == Qt::LeftButton) {
        const QColor &c = m_swatches.value(idx);
        if (c.isValid()) { m_color = c; emit colorChanged(c); update(); }
    } else if (e->button() == Qt::RightButton) {
        showContextMenu(idx, e->globalPosition().toPoint());
    }
}

void ColorSwatchWidget::showContextMenu(int idx, const QPoint &globalPos)
{
    QMenu menu(this);

    auto *saveAct = menu.addAction(tr("Save Current Color"));
    { QPixmap px(16,16); px.fill(m_color); saveAct->setIcon(QIcon(px)); }

    auto *clearAct = menu.addAction(tr("Clear Slot"));
    clearAct->setEnabled(m_swatches.value(idx).isValid());

    const QAction *chosen = menu.exec(globalPos);
    if (chosen == saveAct)       { m_swatches[idx] = m_color;  update(); }
    else if (chosen == clearAct) { m_swatches[idx] = QColor(); update(); }
}

// ─────────────────────────────────────────────────────────────────────────────
// Tooltip
// ─────────────────────────────────────────────────────────────────────────────
bool ColorSwatchWidget::event(QEvent *e)
{
    if (e->type() == QEvent::ToolTip) {
        auto *he = static_cast<QHelpEvent *>(e);
        const int idx = swatchAt(he->pos());
        if (idx >= 0) {
            const QColor &c = m_swatches.value(idx);
            QToolTip::showText(he->globalPos(),
                c.isValid() ? c.name().toUpper()
                            : tr("Empty — right-click to save"));
            return true;
        }
    }
    return QWidget::event(e);
}
