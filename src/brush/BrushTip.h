#pragma once

#include <QImage>
#include <QPointF>
#include <QColor>
#include <QPainter>
#include <QString>
#include <cmath>

// ─────────────────────────────────────────────────────────────────────────────
// DabParams  –  parameters passed to BrushTip::stamp() for a single dab
// ─────────────────────────────────────────────────────────────────────────────

struct DabParams
{
    // ── Core dab geometry & colour ────────────────────────────────────────────
    QPointF center;
    float   diameter;
    float   opacity;
    float   hardness;
    float   pressure;
    QColor  color;

    // ── Wet-media controls ────────────────────────────────────────────────────
    float blending        = 0.0f;
    float dilution        = 0.0f;
    float persistence     = 0.5f;
    float blurPressure    = 0.0f;
    float coloring        = 0.0f;
    float uncolorPressure = 0.0f;
    float blurWidth       = 0.5f;

    // ── Misc ──────────────────────────────────────────────────────────────────
    bool  keepOpacity    = false;
    int   brushShape     = 0;       // 0 = Circle, 1 = Diamond, 2 = Square
    float textureStrength = 1.0f;   // 0 = invisible, 1 = full strength

    // Optional shape BMP applied by the tip (may be nullptr).
    const QImage *shapeMask = nullptr;
};

// ─────────────────────────────────────────────────────────────────────────────
// BrushTip  –  abstract dab-rendering interface
// ─────────────────────────────────────────────────────────────────────────────

class BrushTip
{
public:
    virtual ~BrushTip() = default;

    virtual void   stamp(QPainter &p, const DabParams &dab) = 0;
    virtual QImage preview()     { return {}; }
    virtual void   beginStroke() {}
};

// ─────────────────────────────────────────────────────────────────────────────
// Concrete tip implementations
// ─────────────────────────────────────────────────────────────────────────────

class PixelTip : public BrushTip
{
public:
    void stamp(QPainter &p, const DabParams &dab) override;
};

class EraserTip : public BrushTip
{
public:
    void stamp(QPainter &p, const DabParams &dab) override;
};

class AirbrushTip : public BrushTip
{
public:
    void stamp(QPainter &p, const DabParams &dab) override;
};

// ─── BrushTipWet (SAI "Brush") ───────────────────────────────────────────────
class BrushTipWet : public BrushTip
{
public:
    void beginStroke() override { m_hasSample = false; }
    void stamp(QPainter &p, const DabParams &dab) override;

private:
    QColor m_sample;
    bool   m_hasSample = false;

    // Per-size mask cache
    mutable QImage m_maskCache;
    mutable int    m_lastD   = -1;
    mutable float  m_lastH   = -1.f;

    // Per-size shape cache
    mutable QImage m_shapeCache;
    mutable int    m_shapeD  = -1;
};

// ─── WaterColorTip (SAI "Water Color") ───────────────────────────────────────
class WaterColorTip : public BrushTip
{
public:
    void beginStroke() override { m_hasSample = false; m_dabCount = 0; }
    void stamp(QPainter &p, const DabParams &dab) override;

private:
    QColor m_sample;
    bool   m_hasSample = false;
    int    m_dabCount  = 0;

    // Per-size mask cache
    mutable QImage m_maskCache;
    mutable int    m_lastD  = -1;

    // Per-size shape cache
    mutable QImage m_shapeCache;
    mutable int    m_shapeD = -1;

    static QImage boxBlur(const QImage &src, int radius);
};

// ─── MarkerTip (SAI "Marker") ─────────────────────────────────────────────────
class MarkerTip : public BrushTip
{
public:
    void beginStroke() override { m_hasSample = false; }
    void stamp(QPainter &p, const DabParams &dab) override;

private:
    QColor m_sample;
    bool   m_hasSample = false;

    // Per-size mask cache
    mutable QImage m_maskCache;
    mutable int    m_lastD  = -1;

    // Per-size shape cache
    mutable QImage m_shapeCache;
    mutable int    m_shapeD = -1;
};

// ─── SmudgeTip (SAI "Smudge") ─────────────────────────────────────────────────
class SmudgeTip : public BrushTip
{
public:
    void beginStroke() override { m_hasSample = false; }
    void stamp(QPainter &p, const DabParams &dab) override;

private:
    QImage m_sample;
    bool   m_hasSample = false;

    mutable QImage m_maskCache;
    mutable int    m_lastD = -1;
};

// ─── BlurTip (SAI "Blur") ─────────────────────────────────────────────────────
class BlurTip : public BrushTip
{
public:
    void stamp(QPainter &p, const DabParams &dab) override;

private:
    static QImage boxBlur(const QImage &src, int radius);
};

// ─── ChalkTip ─────────────────────────────────────────────────────────────────
class ChalkTip : public BrushTip
{
public:
    void stamp(QPainter &p, const DabParams &dab) override;
};

// ─────────────────────────────────────────────────────────────────────────────
// TextureTip  –  bitmap brush shape + bitmap texture overlay
//
//   shapePath   : grayscale BMP/PNG used as dab alpha mask (replaces the
//                 default soft circle).
//   texturePath : grayscale BMP/PNG multiplied over the shape (paper / grain
//                 effect).
//
// Either path may be empty; missing inputs fall back to a soft circle / no
// texture respectively.
// ─────────────────────────────────────────────────────────────────────────────
class TextureTip : public BrushTip
{
public:
    explicit TextureTip(const QString &texturePath = {},
                        const QString &shapePath   = {});

    void setTexture(const QString &path);
    void setShape(const QString &path);

    // Public wrapper so BrushEngine can load shape masks for wet tips.
    static QImage loadGrayscaleMaskPublic(const QString &path)
    {
        return loadGrayscaleMask(path, 128);
    }

    void stamp(QPainter &p, const DabParams &dab) override;

private:
    QImage m_shape;
    QImage m_texture;

    static QImage loadGrayscaleMask(const QString &path, int targetSize);
};
