#include "BrushTip.h"

// ─── PixelTip ─────────────────────────────────────────────────────────────────
void PixelTip::stamp(QPainter &p, const DabParams &dab)
{
    if (dab.diameter < 0.5f) return;

    const float r  = dab.diameter * 0.5f;
    const int   d  = static_cast<int>(std::ceil(dab.diameter)) + 2;
    const float cx = d * 0.5f;
    const float cy = d * 0.5f;

    QImage mask(d, d, QImage::Format_ARGB32_Premultiplied);
    mask.fill(Qt::transparent);

    const int baseA = static_cast<int>(dab.opacity * 255.0f);
    const QColor &c = dab.color;

    for (int y = 0; y < d; ++y)
    {
        QRgb *row = reinterpret_cast<QRgb *>(mask.scanLine(y));
        for (int x = 0; x < d; ++x)
        {
            const float dx   = x - cx;
            const float dy   = y - cy;
            const float dist = std::sqrt(dx * dx + dy * dy);
            if (dist >= r) continue;

            float alpha = 1.0f;
            const float hardCore = r * dab.hardness;
            if (dist > hardCore)
            {
                const float t = (dist - hardCore) / std::max(r - hardCore, 0.001f);
                alpha = 0.5f * (1.0f + std::cos(t * 3.14159265f));
            }

            const int a = static_cast<int>(alpha * baseA);
            row[x] = qPremultiply(qRgba(c.red(), c.green(), c.blue(), a));
        }
    }

    p.drawImage(dab.center - QPointF(cx, cy), mask);
}

// ─── EraserTip ────────────────────────────────────────────────────────────────
void EraserTip::stamp(QPainter &p, const DabParams &dab)
{
    PixelTip pt;
    pt.stamp(p, dab);
}

// ─── AirbrushTip ──────────────────────────────────────────────────────────────
void AirbrushTip::stamp(QPainter &p, const DabParams &dab)
{
    DabParams soft = dab;
    soft.hardness  = 0.0f;
    PixelTip pt;
    pt.stamp(p, soft);
}

// ─── SmudgeTip ────────────────────────────────────────────────────────────────
// Picks up a circular sample from the canvas at the dab position, then draws
// it slightly offset — creating a drag/smear. Each stamp re-samples so colour
// bleeds forward naturally.
void SmudgeTip::stamp(QPainter &p, const DabParams &dab)
{
    const int d = static_cast<int>(std::ceil(dab.diameter)) + 2;
    const QPointF topLeft = dab.center - QPointF(d * 0.5, d * 0.5);

    // ── 1. Sample the canvas pixels under the dab ──────────────────────────
    // p.device() is the layer QImage; grab the region the dab covers.
    const QImage *canvas = static_cast<const QImage *>(p.device());
    QImage current = canvas->copy(QRect(topLeft.toPoint(), QSize(d, d)));

    if (!m_hasSample)
    {
        m_sample   = current;
        m_hasSample = true;
    }

    // ── 2. Blend sample toward current canvas (mix = smudge strength) ─────
    // opacity here acts as smudge strength: 1.0 = full colour drag, 0 = none.
    const float mix = dab.opacity * dab.pressure;

    // Build a mask: circle with soft edge, same as PixelTip
    QImage mask(d, d, QImage::Format_ARGB32_Premultiplied);
    mask.fill(Qt::transparent);
    const float r        = dab.diameter * 0.5f;
    const float cx       = d * 0.5f;
    const float cy       = d * 0.5f;
    const float hardCore = r * dab.hardness;

    for (int y = 0; y < d; ++y)
    {
        QRgb *row = reinterpret_cast<QRgb *>(mask.scanLine(y));
        const QRgb *sampleRow  = reinterpret_cast<const QRgb *>(m_sample.scanLine(
                                     std::clamp(y, 0, m_sample.height() - 1)));
        for (int x = 0; x < d; ++x)
        {
            const float dx   = x - cx, dy = y - cy;
            const float dist = std::sqrt(dx * dx + dy * dy);
            if (dist >= r) continue;

            float alpha = 1.0f;
            if (dist > hardCore)
            {
                const float t = (dist - hardCore) / std::max(r - hardCore, 0.001f);
                alpha = 0.5f * (1.0f + std::cos(t * 3.14159265f));
            }

            const int srcX = std::clamp(x, 0, m_sample.width() - 1);
            QRgb sc = sampleRow[srcX];

            // Premultiplied ARGB — scale alpha by mix * edge alpha
            const int finalA = static_cast<int>(qAlpha(sc) * mix * alpha);
            row[x] = qPremultiply(qRgba(qRed(sc), qGreen(sc), qBlue(sc), finalA));
        }
    }

    p.drawImage(topLeft, mask);

    // ── 3. Update stored sample for next stamp (colour bleeds forward) ─────
    m_sample = canvas->copy(QRect(topLeft.toPoint(), QSize(d, d)));
}

// ─── ChalkTip ─────────────────────────────────────────────────────────────────
// Grainy, dry-media feel. Uses a random sparse dot pattern inside the dab
// circle. Grain density scales with pressure so pressing harder gives denser
// coverage, lifting the stylus gives a rough, broken edge.
void ChalkTip::stamp(QPainter &p, const DabParams &dab)
{
    if (dab.diameter < 0.5f) return;

    const int   d  = static_cast<int>(std::ceil(dab.diameter)) + 2;
    const float r  = dab.diameter * 0.5f;
    const float cx = d * 0.5f;
    const float cy = d * 0.5f;

    QImage mask(d, d, QImage::Format_ARGB32_Premultiplied);
    mask.fill(Qt::transparent);

    // density: at full pressure fill ~70% of pixels, at zero pressure ~15%
    const float density = 0.15f + 0.55f * dab.pressure;
    const int   baseA   = static_cast<int>(dab.opacity * 255.0f);
    const QColor &c     = dab.color;

    // Simple LCG for speed — no need for std::mt19937 per dab
    uint32_t rng = static_cast<uint32_t>(dab.center.x() * 1000 + dab.center.y());

    auto nextFloat = [&]() -> float {
        rng = rng * 1664525u + 1013904223u;
        return (rng >> 8) / float(1 << 24);
    };

    for (int y = 0; y < d; ++y)
    {
        QRgb *row = reinterpret_cast<QRgb *>(mask.scanLine(y));
        for (int x = 0; x < d; ++x)
        {
            const float dx   = x - cx, dy = y - cy;
            const float dist = std::sqrt(dx * dx + dy * dy);
            if (dist >= r) continue;

            if (nextFloat() > density) continue;   // skip — grain gap

            // Soft falloff toward edge
            float alpha = 1.0f;
            const float hardCore = r * dab.hardness;
            if (dist > hardCore)
            {
                const float t = (dist - hardCore) / std::max(r - hardCore, 0.001f);
                alpha = 0.5f * (1.0f + std::cos(t * 3.14159265f));
            }

            const int a = static_cast<int>(alpha * baseA);
            row[x] = qPremultiply(qRgba(c.red(), c.green(), c.blue(), a));
        }
    }

    p.drawImage(dab.center - QPointF(cx, cy), mask);
}