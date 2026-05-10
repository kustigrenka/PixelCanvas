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
            const float dist2 = dx * dx + dy * dy;
            if (dist2 >= r * r) continue;   // early reject without sqrt
            const float dist = std::sqrt(dist2);  // only computed for pixels inside the circle
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
            const float dist2 = dx * dx + dy * dy;
            if (dist2 >= r * r) continue;   // early reject without sqrt
            const float dist = std::sqrt(dist2);  // only computed for pixels inside the circle
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

    const float r   = dab.diameter * 0.5f;
    const int   d   = static_cast<int>(std::ceil(dab.diameter)) + 2;
    const int baseA = static_cast<int>(dab.opacity * 255.0f);

    if (dab.brushShape == 1 || dab.brushShape == 2)
    {
        // Diamond or Square — build mask directly without circle clip
        QImage mask(d, d, QImage::Format_ARGB32_Premultiplied);
        mask.fill(Qt::transparent);

        const float cx = d * 0.5f;
        const float cy = d * 0.5f;
        const float hardCore = r * dab.hardness;
        const float softEdge = std::max(r - hardCore, 0.001f);

        for (int y = 0; y < d; ++y)
        {
            QRgb *row = reinterpret_cast<QRgb *>(mask.scanLine(y));
            for (int x = 0; x < d; ++x)
            {
                const float dx = std::abs(x - cx);
                const float dy = std::abs(y - cy);

                float dist;
                if (dab.brushShape == 1)
                    dist = dx + dy;           // L1 norm → diamond
                else
                    dist = std::max(dx, dy);  // L∞ norm → square

                if (dist >= r) continue;

                float alpha = 1.0f;
                if (dist > hardCore)
                {
                    const float t = (dist - hardCore) / softEdge;
                    alpha = 0.5f * (1.0f + std::cos(t * 3.14159265f));
                }

                const int a = static_cast<int>(alpha * baseA);
                row[x] = qPremultiply(qRgba(dab.color.red(), dab.color.green(),
                                            dab.color.blue(), a));
            }
        }
        p.drawImage(dab.center - QPointF(d * 0.5, d * 0.5), mask);
        return;
    }

    // Circle (default) — use cached soft-circle mask
    static QImage cachedMask;
    static int    lastD    = -1;
    static float  lastH    = -1.f;
    static QColor lastC;
    static int    lastA    = -1;

    if (d != lastD || dab.hardness != lastH || dab.color != lastC || baseA != lastA)
    {
        cachedMask = makeSoftCircleMask(d, r, dab.hardness, dab.color, baseA);
        lastD = d; lastH = dab.hardness; lastC = dab.color; lastA = baseA;
    }

    p.drawImage(dab.center - QPointF(d * 0.5, d * 0.5), cachedMask);
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

    const QImage *canvas = static_cast<const QImage *>(p.device());
    const int cx = static_cast<int>(dab.center.x());
    const int cy = static_cast<int>(dab.center.y());
    QColor canvasSample = Qt::transparent;
    if (canvas && cx >= 0 && cy >= 0 && cx < canvas->width() && cy < canvas->height())
    {
        const QRgb px = reinterpret_cast<const QRgb *>(canvas->constScanLine(cy))[cx];
        canvasSample = QColor(qUnpremultiply(px));
    }

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

    const float effOpacity = dab.opacity * (1.0f - dab.dilution);
    const float r   = dab.diameter * 0.5f;
    const int   d   = static_cast<int>(std::ceil(dab.diameter)) + 2;
    const int baseA = static_cast<int>(effOpacity * 255.0f);

    if (d != m_lastD || dab.hardness != m_lastH)
    {
        m_maskCache = makeSoftCircleMask(d, r, dab.hardness, Qt::white, 255);
        m_lastD = d; m_lastH = dab.hardness;
    }

    // Pre-scale shape mask once per size change
    QImage shapeDab;
    if (dab.shapeMask && !dab.shapeMask->isNull())
    {
        if (d != m_shapeD) {
            m_shapeCache = dab.shapeMask->scaled(d, d, Qt::IgnoreAspectRatio,
                                                  Qt::SmoothTransformation)
                           .convertToFormat(QImage::Format_Grayscale8);
            m_shapeD = d;
        }
        shapeDab = m_shapeCache;
    }

    QImage mask(d, d, QImage::Format_ARGB32_Premultiplied);
    mask.fill(Qt::transparent);
    for (int y = 0; y < d; ++y) {
        QRgb       *dst  = reinterpret_cast<QRgb *>(mask.scanLine(y));
        const QRgb *src  = reinterpret_cast<const QRgb *>(m_maskCache.constScanLine(y));
        const uchar *shp = shapeDab.isNull() ? nullptr : shapeDab.constScanLine(y);
        for (int x = 0; x < d; ++x) {
            float circleA = qAlpha(src[x]) / 255.0f;
            float shapeA  = shp ? (255 - shp[x]) / 255.0f : 1.0f;
            const int a   = static_cast<int>(circleA * shapeA * baseA);
            dst[x] = a > 0 ? qPremultiply(qRgba(paintColor.red(), paintColor.green(),
                                                 paintColor.blue(), a)) : 0;
        }
    }
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
    {
        const QRgb px = reinterpret_cast<const QRgb *>(canvas->constScanLine(cy))[cx];
        canvasSample = QColor(qUnpremultiply(px));
    }

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
    const float r   = dab.diameter * 0.5f;
    const int   d   = static_cast<int>(std::ceil(dab.diameter)) + 2;
    const int baseA = static_cast<int>(effOpacity * 255.0f);

// Cache only the blurred alpha shape — color is applied fresh every dab
    if (d != m_lastD)
    {
        // Build a white soft-circle mask and blur it — no color baked in
        QImage alphaMask(d, d, QImage::Format_ARGB32_Premultiplied);
        alphaMask.fill(Qt::transparent);
        const float hc = r * dab.hardness;
        const float se = std::max(r - hc, 0.001f);
        for (int y = 0; y < d; ++y) {
            QRgb *row = reinterpret_cast<QRgb *>(alphaMask.scanLine(y));
            for (int x = 0; x < d; ++x) {
                const float dx = x - d*0.5f, dy = y - d*0.5f;
                const float dist2 = dx * dx + dy * dy;
                if (dist2 >= r * r) continue;   // early reject without sqrt
                const float dist = std::sqrt(dist2);  // only computed for pixels inside the circle
                if (dist >= r) continue;
                float a = 1.0f;
                if (dist > hc) { const float t=(dist-hc)/se; a=0.5f*(1.0f+std::cos(t*3.14159265f)); }
                row[x] = qPremultiply(qRgba(255,255,255,static_cast<int>(a*255.0f)));
            }
        }
        const int blurRadius = static_cast<int>(dab.blurPressure * r * 0.5f);
        if (blurRadius > 0)
            alphaMask = boxBlur(alphaMask, blurRadius);
        m_maskCache = alphaMask;
        m_lastD = d;
    }

    // Apply color + opacity fresh every dab onto the cached alpha shape
    QImage mask(d, d, QImage::Format_ARGB32_Premultiplied);
    mask.fill(Qt::transparent);

    QImage shapeDab;
    if (dab.shapeMask && !dab.shapeMask->isNull())
    {
        if (d != m_shapeD) {
            m_shapeCache = dab.shapeMask->scaled(d, d, Qt::IgnoreAspectRatio,
                                                  Qt::SmoothTransformation)
                           .convertToFormat(QImage::Format_Grayscale8);
            m_shapeD = d;
        }
        shapeDab = m_shapeCache;
    }
    else
    {
        m_shapeCache = QImage();
        m_shapeD     = -1;
    }

    for (int y = 0; y < d; ++y) {
        QRgb        *dst = reinterpret_cast<QRgb *>(mask.scanLine(y));
        const QRgb  *src = reinterpret_cast<const QRgb *>(m_maskCache.constScanLine(y));
        const uchar *shp = shapeDab.isNull() ? nullptr : shapeDab.constScanLine(y);
        for (int x = 0; x < d; ++x) {
            float circleA = qAlpha(src[x]) / 255.0f;
            float shapeA  = shp ? (255 - shp[x]) / 255.0f : 1.0f;
            const int a   = static_cast<int>(circleA * shapeA * baseA);
            dst[x] = a > 0 ? qPremultiply(qRgba(paintColor.red(), paintColor.green(),
                                                 paintColor.blue(), a)) : 0;
        }
    }
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
    {
        const QRgb px = reinterpret_cast<const QRgb *>(canvas->constScanLine(cy))[cx];
        canvasSample = QColor(qUnpremultiply(px));
    }

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

    const float r   = dab.diameter * 0.5f;
    const int   d   = static_cast<int>(std::ceil(dab.diameter)) + 2;
    const int baseA = static_cast<int>(dab.opacity * 255.0f);

    if (d != m_lastD)
    {
        QImage mask = makeSoftCircleMask(d, r, dab.hardness, paintColor, baseA);
        const int blurRadius = static_cast<int>(dab.blurPressure * dab.pressure * r * 0.5f);
        if (blurRadius > 0)
            mask = doBoxBlur(mask, blurRadius);
        m_maskCache = mask;
        m_lastD = d;
    }

    // Cache scaled shape mask — only rescale when brush size changes
    if (dab.shapeMask && !dab.shapeMask->isNull() && d != m_shapeD)
    {
        m_shapeCache = dab.shapeMask->scaled(d, d, Qt::IgnoreAspectRatio,
                                              Qt::SmoothTransformation)
                       .convertToFormat(QImage::Format_Grayscale8);
        m_shapeD = d;
    }
    if (!dab.shapeMask || dab.shapeMask->isNull())
    {
        m_shapeCache = QImage();
        m_shapeD     = -1;
    }

    if (m_shapeCache.isNull())
    {
        p.drawImage(dab.center - QPointF(d * 0.5, d * 0.5), m_maskCache);
    }
    else
    {
        QImage mask(d, d, QImage::Format_ARGB32_Premultiplied);
        mask.fill(Qt::transparent);
        for (int y = 0; y < d; ++y) {
            QRgb        *dst = reinterpret_cast<QRgb *>(mask.scanLine(y));
            const QRgb  *src = reinterpret_cast<const QRgb *>(m_maskCache.constScanLine(y));
            const uchar *shp = m_shapeCache.constScanLine(y);
            for (int x = 0; x < d; ++x) {
                const float shapeA = (255 - shp[x]) / 255.0f;
                const int a = static_cast<int>(qAlpha(src[x]) * shapeA);
                dst[x] = a > 0 ? qPremultiply(qRgba(qRed(src[x]), qGreen(src[x]),
                                                     qBlue(src[x]), a)) : 0;
            }
        }
        p.drawImage(dab.center - QPointF(d * 0.5, d * 0.5), mask);
    }
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

    if (d != m_lastD) {
        m_maskCache = QImage(d, d, QImage::Format_ARGB32_Premultiplied);
        m_lastD = d;
    }
    m_maskCache.fill(Qt::transparent);
    QImage &mask = m_maskCache;

    for (int y = 0; y < d; ++y)
    {
        QRgb *row = reinterpret_cast<QRgb *>(mask.scanLine(y));
        const QRgb *sampleRow = reinterpret_cast<const QRgb *>(
            m_sample.scanLine(std::clamp(y, 0, m_sample.height() - 1)));

        for (int x = 0; x < d; ++x)
        {
            const float dx   = x - cx, dy = y - cy;
            const float dist2 = dx * dx + dy * dy;
            if (dist2 >= r * r) continue;   // early reject without sqrt
            const float dist = std::sqrt(dist2);  // only computed for pixels inside the circle
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

    // Reuse output buffer — only reallocate when size changes
    static QImage output;
    static int lastBlurD = -1;
    if (d != lastBlurD) {
        output = QImage(d, d, QImage::Format_ARGB32_Premultiplied);
        lastBlurD = d;
    }
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

    // Do NOT override composition mode — BrushEngine sets it before stamp() is called
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
            const float dist2 = dx * dx + dy * dy;
            if (dist2 >= r * r) continue;   // early reject without sqrt
            const float dist = std::sqrt(dist2);  // only computed for pixels inside the circle
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

// ─────────────────────────────────────────────────────────────────────────────
// TextureTip
// ─────────────────────────────────────────────────────────────────────────────
TextureTip::TextureTip(const QString &texturePath, const QString &shapePath)
{
    if (!shapePath.isEmpty())   setShape(shapePath);
    if (!texturePath.isEmpty()) setTexture(texturePath);
}

void TextureTip::setShape(const QString &path)
{
    m_shape = loadGrayscaleMask(path, 128);
}

void TextureTip::setTexture(const QString &path)
{
    m_texture = loadGrayscaleMask(path, 128);
}

QImage TextureTip::loadGrayscaleMask(const QString &path, int /*targetSize*/)
{
    QImage src(path);
    if (src.isNull()) return {};

    // BMPs use white background (255) + dark ink (0–127).
    // Binarize: dark pixels → black (ink), everything else → white (bg).
    // Blue-channel boundary pixels (some exporters) → white (strip them).
    src = src.convertToFormat(QImage::Format_RGB32);
    for (int y = 0; y < src.height(); ++y)
    {
        QRgb *row = reinterpret_cast<QRgb *>(src.scanLine(y));
        for (int x = 0; x < src.width(); ++x)
        {
            const int r  = qRed(row[x]);
            const int g  = qGreen(row[x]);
            const int b_ = qBlue(row[x]);
            const int lum = (r + g + b_) / 3;
            if (b_ - r > 40 && b_ - g > 40) row[x] = qRgb(255, 255, 255);
            else if (lum <= 127)             row[x] = qRgb(0, 0, 0);
            else                             row[x] = qRgb(255, 255, 255);
        }
    }
    return src.convertToFormat(QImage::Format_Grayscale8);
}

void TextureTip::stamp(QPainter &p, const DabParams &dab)
{
    if (dab.diameter < 0.5f) return;

    const int   d  = static_cast<int>(std::ceil(dab.diameter)) + 2;
    const float r  = dab.diameter * 0.5f;
    const float cx = d * 0.5f;
    const float cy = d * 0.5f;

    QImage mask(d, d, QImage::Format_ARGB32_Premultiplied);
    mask.fill(Qt::transparent);

    // Scale shape BMP from native size to dab size — single scaling operation.
    // After bilinear scale, clip near-white ghost pixels (gray > 200 → 255)
    // that would otherwise accumulate into a halo with SourceOver.
    // Gray 1-200 = real ink or legitimate edge AA — kept intact.
    QImage shapeDab;
    if (!m_shape.isNull())
    {
        shapeDab = m_shape.scaled(d, d, Qt::IgnoreAspectRatio,
                                  Qt::SmoothTransformation);
        // shapeDab is still RGB32 after scaled(); convert then clip
        shapeDab = shapeDab.convertToFormat(QImage::Format_Grayscale8);
        for (int sy = 0; sy < shapeDab.height(); ++sy)
        {
            uchar *srow = shapeDab.scanLine(sy);
            for (int sx = 0; sx < shapeDab.width(); ++sx)
                if (srow[sx] > 200) srow[sx] = 255;
        }
    }

    const int    baseA    = static_cast<int>(dab.opacity * 255.0f);
    const QColor &c       = dab.color;
    const float  hardCore = r * dab.hardness;
    const float  softEdge = std::max(r - hardCore, 0.001f);

    // Canvas-space origin for world-space texture tiling
    const int originX = static_cast<int>(std::round(dab.center.x())) - static_cast<int>(cx);
    const int originY = static_cast<int>(std::round(dab.center.y())) - static_cast<int>(cy);
    const int texW = m_texture.isNull() ? 0 : m_texture.width();
    const int texH = m_texture.isNull() ? 0 : m_texture.height();

    for (int y = 0; y < d; ++y)
    {
        QRgb *row = reinterpret_cast<QRgb *>(mask.scanLine(y));
        for (int x = 0; x < d; ++x)
        {
            // ── Circle clip — skip entirely if a shape BMP is active ──────────
            const float dx2  = x - cx;
            const float dy2  = y - cy;
            const float dist2 = dx2 * dx2 + dy2 * dy2;
            if (dist2 >= r * r) { row[x] = 0; continue; }

            float circleAlpha;
            if (shapeDab.isNull())
            {
                // No shape BMP — use soft circle as the dab boundary
                const float dist = std::sqrt(dist2);
                if (dist > hardCore)
                {
                    const float t = (dist - hardCore) / softEdge;
                    circleAlpha = 0.5f * (1.0f + std::cos(t * 3.14159265f));
                }
                else
                {
                    circleAlpha = 1.0f;
                }
            }
            else
            {
                // Shape BMP is active — let the shape define the boundary,
                // no circle softness so no ghost fringe outside the shape
                circleAlpha = 1.0f;
            }

            // ── Shape BMP: dab-local, scaled to brush size ───────────────────
            // Replaces the plain circle with a textured silhouette.
            // Dark pixels in the BMP = ink; white = transparent.
            float shapeAlpha = 1.0f;
            if (!shapeDab.isNull())
            {
                const uchar raw = reinterpret_cast<const uchar *>(
                    shapeDab.constScanLine(y))[x];
                shapeAlpha = (255 - raw) / 255.0f;
            }

            // ── Texture BMP: world-space tiled overlay ───────────────────────
            // Tiled across the canvas so the paper/grain stays fixed while
            // the brush reveals it. Applied as a multiplier over the shape.
            float texAlpha = 1.0f;
            if (texW > 0 && texH > 0)
            {
                const int tx = ((originX + x) % texW + texW) % texW;
                const int ty = ((originY + y) % texH + texH) % texH;
                const uchar raw = reinterpret_cast<const uchar *>(
                    m_texture.constScanLine(ty))[tx];
                // raw=0 → full ink, raw=255 → transparent.
                // textureStrength blends between no-texture (1.0) and full-texture.
                const float t = (255 - raw) / 255.0f;
                texAlpha = 1.0f - dab.textureStrength * (1.0f - t);
            }

            const int a = std::clamp(
                static_cast<int>(circleAlpha * shapeAlpha * texAlpha * baseA),
                0, 255);
            row[x] = qPremultiply(qRgba(c.red(), c.green(), c.blue(), a));
        }
    }

    p.drawImage(dab.center - QPointF(cx, cy), mask);
}