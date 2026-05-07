#include "BrushPreviewWidget.h"

#include <QPainter>
#include <cmath>
#include <algorithm>

BrushPreviewWidget::BrushPreviewWidget(QWidget *parent)
    : QWidget(parent)
{
    setFixedHeight(56);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void BrushPreviewWidget::updatePreview(const QColor &color, float size,
                                        float opacity, float hardness)
{
    m_color    = color;
    m_size     = size;
    m_opacity  = opacity;
    m_hardness = hardness;
    update();
}

void BrushPreviewWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), QColor("#1a1a1a"));
    p.setRenderHint(QPainter::Antialiasing);

    const int   W     = width();
    const int   H     = height();
    const int   steps = W / 3;
    const float diam  = std::clamp(m_size, 2.0f, static_cast<float>(H - 10));
    const int   passes = (m_hardness < 0.5f) ? 6 : 2;

    QColor c = m_color;
    c.setAlphaF(std::clamp(static_cast<double>(m_opacity / passes), 0.0, 1.0));
    p.setPen(Qt::NoPen);
    p.setBrush(c);

    for (int i = 0; i < steps; ++i)
    {
        float t     = i / static_cast<float>(steps - 1);
        float taper = 1.0f - std::abs(2.0f * t - 1.0f) * 0.45f;
        float x     = (i * W) / static_cast<float>(steps)
                    + W / static_cast<float>(steps) / 2.0f;
        float r     = diam * taper * 0.5f;
        for (int pass = 0; pass < passes; ++pass)
            p.drawEllipse(QPointF(x, H / 2.0), r, r);
    }
}
