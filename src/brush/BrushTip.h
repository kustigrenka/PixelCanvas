#pragma once

#include <QImage>
#include <QPointF>
#include <QColor>
#include <QPainter>
#include <QString>
#include <cmath>

// ─────────────────────────────────────────────────────────────────────────────
// DabParams
// ─────────────────────────────────────────────────────────────────────────────
struct DabParams
{
    QPointF  center;
    float    diameter;
    float    opacity;
    float    hardness;
    float    pressure;
    QColor   color;

    float    blending        = 0.0f;
    float    dilution        = 0.0f;
    float    persistence     = 0.5f;
    float    blurPressure    = 0.0f;
    float    coloring        = 0.0f;
    float    uncolorPressure = 0.0f;
    float    blurWidth       = 0.5f;
    bool     keepOpacity     = false;
    int      brushShape      = 0;   // 0=Circle  1=Diamond  2=Square
};

// ─────────────────────────────────────────────────────────────────────────────
// BrushTip  –  abstract interface
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
// Concrete tips
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

class BrushTipWet : public BrushTip
{
public:
    void beginStroke() override { m_hasSample = false; }
    void stamp(QPainter &p, const DabParams &dab) override;
private:
    QColor m_sample;
    bool   m_hasSample = false;
};

class WaterColorTip : public BrushTip
{
public:
    void beginStroke() override { m_hasSample = false; }
    void stamp(QPainter &p, const DabParams &dab) override;
private:
    QColor m_sample;
    bool   m_hasSample = false;
    static QImage boxBlur(const QImage &src, int radius);
};

class MarkerTip : public BrushTip
{
public:
    void beginStroke() override { m_hasSample = false; }
    void stamp(QPainter &p, const DabParams &dab) override;
private:
    QColor m_sample;
    bool   m_hasSample = false;
};

class SmudgeTip : public BrushTip
{
public:
    void beginStroke() override { m_hasSample = false; }
    void stamp(QPainter &p, const DabParams &dab) override;
private:
    QImage m_sample;
    bool   m_hasSample = false;
};

class BlurTip : public BrushTip
{
public:
    void stamp(QPainter &p, const DabParams &dab) override;
private:
    static QImage boxBlur(const QImage &src, int radius);
};

class ChalkTip : public BrushTip
{
public:
    void stamp(QPainter &p, const DabParams &dab) override;
};

// ─────────────────────────────────────────────────────────────────────────────
// TextureTip  –  bitmap brush shape + bitmap texture overlay
//
// shapePath:    grayscale BMP/PNG used as dab alpha mask (replaces soft circle)
// texturePath:  grayscale BMP/PNG multiplied over the shape (paper/grain effect)
// Either path may be empty — falls back to soft circle / no texture.
// ─────────────────────────────────────────────────────────────────────────────
class TextureTip : public BrushTip
{
public:
    explicit TextureTip(const QString &texturePath = {},
                        const QString &shapePath   = {});

    void setTexture(const QString &path);
    void setShape(const QString &path);

    void stamp(QPainter &p, const DabParams &dab) override;

private:
    QImage m_shape;    // Grayscale8
    QImage m_texture;  // Grayscale8

    static QImage loadGrayscaleMask(const QString &path, int targetSize);
};
