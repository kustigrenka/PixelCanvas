#include "BrushTip.h"
#include <algorithm>

// ─────────────────────────────────────────────────────────────────────────────
// Internal helpers
// ─────────────────────────────────────────────────────────────────────────────

namespace {

// Build a soft-circle alpha mask.
// Each pixel value is premultiplied ARGB with the given colour at the
// computed alpha.  Used by most tips.
QImage makeSoftCircleMask(int d, float r, float hardness,
                           const QColor &color, int baseA)
{
    QImage mask(d, d, QImage::Format_ARGB32_Premultiplied);
    mask.fill(Qt::transparent);

    const float cx       = d * 0.5f;
    const float cy       = d * 0.5f;
    const float hardCore = r * hardness;
    const float softEdge = std::max(r - hardCore, 0.001f);

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
            if (dist > hardCore)
            {
                const float t = (dist - hardCore) / softEdge;
                alpha = 0.5f * (1.0f + std::cos(t * 3.14159265f));
            }

            const int a = static_cast<int>(alpha * baseA);
            row[x] = qPremultiply(qRgba(color.red(), color.green(), color.blue(), a));
        }
    }
    return mask;
}

// Per-pixel alpha mask (0–255 float, no colour baked in).
// Used by tips that need to composite colours themselves.
QImage makeAlphaMask(int d, float r, float hardness)
{
    QImage mask(d, d, QImage::Format_ARGB32_Premultiplied);
    mask.fill(Qt::transparent);

    const float cx       = d * 0.5f;
    const float cy       = d * 0.5f;
    const float hardCore = r * hardness;
    const float softEdge = std::max(r - hardCore, 0.001f);

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
            if (dist > hardCore)
            {
                const float t = (dist - hardCore) / softEdge;
                alpha = 0.5f * (1.0f + std::cos(t * 3.14159265f));
            }

            const int a = static_cast<int>(alpha * 255.0f);
            row[x] = qPremultiply(qRgba(255, 255, 255, a));
        }
    }
    return mask;
}

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
// PixelTip  (SAI "Pencil")
// ─────────────────────────────────────────────────────────────────────────────
void PixelTip::stamp(QPainter &p, const DabParams &dab)
{
    if (dab.diameter < 0.5f) return;

    const float r  = dab.diameter * 0.5f;
    const int   d  = static_cast<int>(std::ceil(dab.diameter)) + 2;
    const int baseA = static_cast<int>(dab.opacity * 255.0f);

    const QImage mask = makeSoftCircleMask(d, r, dab.hardness, dab.color, baseA);
    p.drawImage(dab.center - QPointF(d * 0.5, d * 0.5), mask);
}

// ─────────────────────────────────────────────────────────────────────────────
// EraserTip  (SAI "Eraser")
// ─────────────────────────────────────────────────────────────────────────────
void EraserTip::stamp(QPainter &p, const DabParams &dab)
{
    // Shape is identical to PixelTip.  BrushEngine already set the composition
    // mode to DestinationOut, so drawing any colour erases the layer.
    PixelTip pt;
    pt.stamp(p, dab);
}

// ─────────────────────────────────────────────────────────────────────────────
// AirbrushTip  (SAI "AirBrush")
// ─────────────────────────────────────────────────────────────────────────────
void AirbrushTip::stamp(QPainter &p, const DabParams &dab)
{
    DabParams soft = dab;
    soft.hardness  = 0.0f;   // airbrush is always soft
    PixelTip pt;
    pt.stamp(p, soft);
}

// ─────────────────────────────────────────────────────────────────────────────
// BrushTipWet  (SAI "Brush")
//
// At each dab:
//   1. Sample the average canvas colour under the dab centre.
//   2. Mix the stored sample colour toward the new canvas sample
//      (persistence controls how slowly the sample updates).
//   3. Blend the sample colour with the foreground colour
//      (blending: 0 = pure FG, 1 = pure canvas pickup).
//   4. Apply dilution as opacity reduction.
//   5. Paint the resulting colour using a soft-circle mask.
// ─────────────────────────────────────────────────────────────────────────────
void BrushTipWet::stamp(QPainter &p, const DabParams &dab)
{
    if (dab.diameter < 0.5f) return;

    // ── 1. Sample canvas colour under dab centre ───────────────────────────
    const QImage *canvas = static_cast<const QImage *>(p.device());
    const int cx = static_cast<int>(dab.center.x());
    const int cy = static_cast<int>(dab.center.y());
    QColor canvasSample = Qt::transparent;
    if (canvas && cx >= 0 && cy >= 0 && cx < canvas->width() && cy < canvas->height())
        canvasSample = canvas->pixelColor(cx, cy);

    // ── 2. Update stored sample (persistence) ─────────────────────────────
    if (!m_hasSample)
    {
        m_sample   = canvasSample;
        m_hasSample = true;
    }
    else
    {
        // Lerp stored sample toward current canvas sample, speed = 1-persistence
        const float refresh = 1.0f - dab.persistence;
        m_sample = QColor(
            static_cast<int>(m_sample.red()   + refresh * (canvasSample.red()   - m_sample.red())),
            static_cast<int>(m_sample.green() + refresh * (canvasSample.green() - m_sample.green())),
            static_cast<int>(m_sample.blue()  + refresh * (canvasSample.blue()  - m_sample.blue())),
            255
        );
    }

    // ── 3. Mix FG colour with canvas sample (blending) ────────────────────
    const float b = dab.blending;
    QColor paintColor(
        static_cast<int>(dab.color.red()   * (1.0f - b) + m_sample.red()   * b),
        static_cast<int>(dab.color.green() * (1.0f - b) + m_sample.green() * b),
        static_cast<int>(dab.color.blue()  * (1.0f - b) + m_sample.blue()  * b)
    );

    // ── 4. Dilution reduces effective opacity ─────────────────────────────
    const float effOpacity = dab.opacity * (1.0f - dab.dilution);

    // ── 5. Paint mask ──────────────────────────────────────────────────────
    const float r  = dab.diameter * 0.5f;
    const int   d  = static_cast<int>(std::ceil(dab.diameter)) + 2;
    const int baseA = static_cast<int>(effOpacity * 255.0f);

    const QImage mask = makeSoftCircleMask(d, r, dab.hardness, paintColor, baseA);
    p.drawImage(dab.center - QPointF(d * 0.5, d * 0.5), mask);
}

// ─────────────────────────────────────────────────────────────────────────────
// Internal box-blur helper shared by WaterColorTip and BlurTip
// ─────────────────────────────────────────────────────────────────────────────
static QImage doBoxBlur(const QImage &src, int radius)
{
    if (radius < 1) return src;
    const int w = src.width(), h = src.height();
    QImage tmp(w, h, QImage::Format_ARGB32_Premultiplied);
    QImage dst(w, h, QImage::Format_ARGB32_Premultiplied);
    tmp.fill(Qt::transparent);
    dst.fill(Qt::transparent);

    const int span = 2 * radius + 1;

    // Horizontal pass
    for (int y = 0; y < h; ++y)
    {
        const QRgb *s = reinterpret_cast<const QRgb *>(src.constScanLine(y));
        QRgb       *d = reinterpret_cast<QRgb *>(tmp.scanLine(y));
        for (int x = 0; x < w; ++x)
        {
            int r = 0, g = 0, b = 0, a = 0, cnt = 0;
            for (int k = -radius; k <= radius; ++k)
            {
                const int sx = std::clamp(x + k, 0, w - 1);
                const QRgb p = s[sx];
                r += qRed(p); g += qGreen(p); b += qBlue(p); a += qAlpha(p);
                ++cnt;
            }
            d[x] = qRgba(r / cnt, g / cnt, b / cnt, a / cnt);
        }
    }

    // Vertical pass
    for (int x = 0; x < w; ++x)
    {
        for (int y = 0; y < h; ++y)
        {
            int r = 0, g = 0, b = 0, a = 0, cnt = 0;
            for (int k = -radius; k <= radius; ++k)
            {
                const int sy = std::clamp(y + k, 0, h - 1);
                const QRgb p = reinterpret_cast<const QRgb *>(tmp.constScanLine(sy))[x];
                r += qRed(p); g += qGreen(p); b += qBlue(p); a += qAlpha(p);
                ++cnt;
            }
            reinterpret_cast<QRgb *>(dst.scanLine(y))[x] =
                qRgba(r / cnt, g / cnt, b / cnt, a / cnt);
        }
    }
    return dst;
}

// ─────────────────────────────────────────────────────────────────────────────
// WaterColorTip  (SAI "Water Color")
//
// Same as BrushTipWet but applies a post-blur to the dab mask so wet edges
// feather into the canvas.  blurPressure * pressure controls the blur radius.
// ─────────────────────────────────────────────────────────────────────────────
QImage WaterColorTip::boxBlur(const QImage &src, int radius)
{
    return doBoxBlur(src, radius);
}

void WaterColorTip::stamp(QPainter &p, const DabParams &dab)
{
    if (dab.diameter < 0.5f) return;

    const QImage *canvas = static_cast<const QImage *>(p.device());
    const int cx = static_cast<int>(dab.center.x());
    const int cy = static_cast<int>(dab.center.y());
    QColor canvasSample = Qt::transparent;
    if (canvas && cx >= 0 && cy >= 0 && cx < canvas->width() && cy < canvas->height())
        canvasSample = canvas->pixelColor(cx, cy);

    if (!m_hasSample)
    {
        m_sample   = canvasSample;
        m_hasSample = true;
    }
    else
    {
        const float refresh = 1.0f - dab.persistence;
        m_sample = QColor(
            static_cast<int>(m_sample.red()   + refresh * (canvasSample.red()   - m_sample.red())),
            static_cast<int>(m_sample.green() + refresh * (canvasSample.green() - m_sample.green())),
            static_cast<int>(m_sample.blue()  + refresh * (canvasSample.blue()  - m_sample.blue())),
            255
        );
    }

    const float b = dab.blending;
    QColor paintColor(
        static_cast<int>(dab.color.red()   * (1.0f - b) + m_sample.red()   * b),
        static_cast<int>(dab.color.green() * (1.0f - b) + m_sample.green() * b),
        static_cast<int>(dab.color.blue()  * (1.0f - b) + m_sample.blue()  * b)
    );

    const float effOpacity = dab.opacity * (1.0f - dab.dilution);
    const float r  = dab.diameter * 0.5f;
    const int   d  = static_cast<int>(std::ceil(dab.diameter)) + 2;
    const int baseA = static_cast<int>(effOpacity * 255.0f);

    QImage mask = makeSoftCircleMask(d, r, dab.hardness, paintColor, baseA);

    // ── Blur the mask proportionally to blurPressure * pressure ───────────
    const int blurRadius = static_cast<int>(dab.blurPressure * dab.pressure * r * 0.5f);
    if (blurRadius > 0)
        mask = boxBlur(mask, blurRadius);

    p.drawImage(dab.center - QPointF(d * 0.5, d * 0.5), mask);
}

// ─────────────────────────────────────────────────────────────────────────────
// MarkerTip  (SAI "Marker")
//
// Flat opacity (ignores pressure→opacity), blending + persistence like
// BrushTipWet, blur pressure for feathered edges.  No dilution.
// ─────────────────────────────────────────────────────────────────────────────
void MarkerTip::stamp(QPainter &p, const DabParams &dab)
{
    if (dab.diameter < 0.5f) return;

    const QImage *canvas = static_cast<const QImage *>(p.device());
    const int cx = static_cast<int>(dab.center.x());
    const int cy = static_cast<int>(dab.center.y());
    QColor canvasSample = Qt::transparent;
    if (canvas && cx >= 0 && cy >= 0 && cx < canvas->width() && cy < canvas->height())
        canvasSample = canvas->pixelColor(cx, cy);

    if (!m_hasSample)
    {
        m_sample    = canvasSample;
        m_hasSample = true;
    }
    else
    {
        const float refresh = 1.0f - dab.persistence;
        m_sample = QColor(
            static_cast<int>(m_sample.red()   + refresh * (canvasSample.red()   - m_sample.red())),
            static_cast<int>(m_sample.green() + refresh * (canvasSample.green() - m_sample.green())),
            static_cast<int>(m_sample.blue()  + refresh * (canvasSample.blue()  - m_sample.blue())),
            255
        );
    }

    const float b = dab.blending;
    QColor paintColor(
        static_cast<int>(dab.color.red()   * (1.0f - b) + m_sample.red()   * b),
        static_cast<int>(dab.color.green() * (1.0f - b) + m_sample.green() * b),
        static_cast<int>(dab.color.blue()  * (1.0f - b) + m_sample.blue()  * b)
    );

    // Marker: fixed opacity regardless of pressure (that's what makes it feel like a marker)
    const float r   = dab.diameter * 0.5f;
    const int   d   = static_cast<int>(std::ceil(dab.diameter)) + 2;
    const int baseA = static_cast<int>(dab.opacity * 255.0f);

    QImage mask = makeSoftCircleMask(d, r, dab.hardness, paintColor, baseA);

    const int blurRadius = static_cast<int>(dab.blurPressure * dab.pressure * r * 0.5f);
    if (blurRadius > 0)
        mask = doBoxBlur(mask, blurRadius);

    p.drawImage(dab.center - QPointF(d * 0.5, d * 0.5), mask);
}

// ─────────────────────────────────────────────────────────────────────────────
// SmudgeTip  (SAI "Smudge")
// ─────────────────────────────────────────────────────────────────────────────
void SmudgeTip::stamp(QPainter &p, const DabParams &dab)
{
    const int d = static_cast<int>(std::ceil(dab.diameter)) + 2;
    const QPointF topLeft = dab.center - QPointF(d * 0.5, d * 0.5);

    const QImage *canvas = static_cast<const QImage *>(p.device());
    QImage current = canvas->copy(QRect(topLeft.toPoint(), QSize(d, d)));

    if (!m_hasSample)
    {
        m_sample    = current;
        m_hasSample = true;
    }

    const float r        = dab.diameter * 0.5f;
    const float cx       = d * 0.5f;
    const float cy       = d * 0.5f;
    const float hardCore = r * dab.hardness;
    const float softEdge = std::max(r - hardCore, 0.001f);

    // ── Build output: mix sample toward canvas colour ──────────────────────
    // coloring: inject foreground colour into the smear
    // uncolorPressure: at high pressure, reduce sample alpha (erase)
    const float smudgeStrength = dab.opacity * dab.pressure;
    const float colorMix       = dab.coloring;
    const float uncolor        = dab.uncolorPressure * dab.pressure;

    QImage mask(d, d, QImage::Format_ARGB32_Premultiplied);
    mask.fill(Qt::transparent);

    for (int y = 0; y < d; ++y)
    {
        QRgb *row = reinterpret_cast<QRgb *>(mask.scanLine(y));
        const QRgb *sampleRow = reinterpret_cast<const QRgb *>(
            m_sample.scanLine(std::clamp(y, 0, m_sample.height() - 1)));

        for (int x = 0; x < d; ++x)
        {
            const float dx   = x - cx, dy = y - cy;
            const float dist = std::sqrt(dx * dx + dy * dy);
            if (dist >= r) continue;

            float edge = 1.0f;
            if (dist > hardCore)
            {
                const float t = (dist - hardCore) / softEdge;
                edge = 0.5f * (1.0f + std::cos(t * 3.14159265f));
            }

            const int srcX = std::clamp(x, 0, m_sample.width() - 1);
            QRgb sc = sampleRow[srcX];

            // Mix in foreground colour
            int sr = static_cast<int>(qRed(sc)   * (1.0f - colorMix) + dab.color.red()   * colorMix);
            int sg = static_cast<int>(qGreen(sc) * (1.0f - colorMix) + dab.color.green() * colorMix);
            int sb = static_cast<int>(qBlue(sc)  * (1.0f - colorMix) + dab.color.blue()  * colorMix);
            int sa = static_cast<int>(qAlpha(sc) * (1.0f - uncolor));

            const int finalA = static_cast<int>(sa * smudgeStrength * edge);
            row[x] = qPremultiply(qRgba(sr, sg, sb, std::clamp(finalA, 0, 255)));
        }
    }

    p.drawImage(topLeft, mask);

    // Update sample for next dab
    m_sample = canvas->copy(QRect(topLeft.toPoint(), QSize(d, d)));
}

// ─────────────────────────────────────────────────────────────────────────────
// BlurTip  (SAI "Blur")
// ─────────────────────────────────────────────────────────────────────────────
QImage BlurTip::boxBlur(const QImage &src, int radius)
{
    return doBoxBlur(src, radius);
}

void BlurTip::stamp(QPainter &p, const DabParams &dab)
{
    if (dab.diameter < 0.5f) return;

    const int   d  = static_cast<int>(std::ceil(dab.diameter)) + 2;
    const float r  = dab.diameter * 0.5f;
    const QPointF topLeft = dab.center - QPointF(d * 0.5, d * 0.5);

    const QImage *canvas = static_cast<const QImage *>(p.device());
    QImage region = canvas->copy(QRect(topLeft.toPoint(), QSize(d, d)));

    // Blur the canvas region
    const int blurRadius = std::max(1, static_cast<int>(dab.blurWidth * r * dab.pressure));
    QImage blurred = boxBlur(region, blurRadius);

    // Use the soft circle alpha mask to blend blurred back onto canvas
    QImage alphaMask = makeAlphaMask(d, r, dab.hardness);

    QImage output(d, d, QImage::Format_ARGB32_Premultiplied);
    output.fill(Qt::transparent);

    for (int y = 0; y < d; ++y)
    {
        QRgb       *out   = reinterpret_cast<QRgb *>(output.scanLine(y));
        const QRgb *blr   = reinterpret_cast<const QRgb *>(blurred.constScanLine(y));
        const QRgb *msk   = reinterpret_cast<const QRgb *>(alphaMask.constScanLine(y));
        const QRgb *orig  = reinterpret_cast<const QRgb *>(region.constScanLine(y));

        for (int x = 0; x < d; ++x)
        {
            const float alpha = qAlpha(msk[x]) / 255.0f * dab.opacity;
            const int r_ = static_cast<int>(qRed(orig[x])   * (1 - alpha) + qRed(blr[x])   * alpha);
            const int g_ = static_cast<int>(qGreen(orig[x]) * (1 - alpha) + qGreen(blr[x]) * alpha);
            const int b_ = static_cast<int>(qBlue(orig[x])  * (1 - alpha) + qBlue(blr[x])  * alpha);
            out[x] = qPremultiply(qRgba(r_, g_, b_, 255));
        }
    }

    // Paint the output directly — use SourceOver so alpha composites correctly
    p.setCompositionMode(QPainter::CompositionMode_SourceOver);
    p.drawImage(topLeft, output);
}

// ─────────────────────────────────────────────────────────────────────────────
// ChalkTip  (extra dry-grain tip)
// ─────────────────────────────────────────────────────────────────────────────
void ChalkTip::stamp(QPainter &p, const DabParams &dab)
{
    if (dab.diameter < 0.5f) return;

    const int   d  = static_cast<int>(std::ceil(dab.diameter)) + 2;
    const float r  = dab.diameter * 0.5f;
    const float cx = d * 0.5f;
    const float cy = d * 0.5f;

    QImage mask(d, d, QImage::Format_ARGB32_Premultiplied);
    mask.fill(Qt::transparent);

    const float density = 0.15f + 0.55f * dab.pressure;
    const int   baseA   = static_cast<int>(dab.opacity * 255.0f);
    const float hardCore = r * dab.hardness;
    const float softEdge = std::max(r - hardCore, 0.001f);

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
            if (dist >= r || nextFloat() > density) continue;

            float alpha = 1.0f;
            if (dist > hardCore)
            {
                const float t = (dist - hardCore) / softEdge;
                alpha = 0.5f * (1.0f + std::cos(t * 3.14159265f));
            }

            const int a = static_cast<int>(alpha * baseA);
            row[x] = qPremultiply(qRgba(dab.color.red(), dab.color.green(), dab.color.blue(), a));
        }
    }

    p.drawImage(dab.center - QPointF(cx, cy), mask);
}
