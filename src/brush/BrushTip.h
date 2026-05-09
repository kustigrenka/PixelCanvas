#pragma once

#include <QImage>
#include <QPointF>
#include <QColor>
#include <QPainter>
#include <cmath>

// ─────────────────────────────────────────────────────────────────────────────
// DabParams  –  everything a BrushTip::stamp() needs for one dab.
//
// BrushEngine computes these from BrushSettings + current pressure and passes
// them to the active tip.  Tips must not read BrushSettings directly — use
// only DabParams so tips stay decoupled from the settings struct.
// ─────────────────────────────────────────────────────────────────────────────
struct DabParams
{
    QPointF  center;          // canvas-space dab centre
    float    diameter;        // effective size at current pressure (px)
    float    opacity;         // effective opacity at current pressure  (0–1)
    float    hardness;        // edge hardness  (0 soft – 1 hard)
    float    pressure;        // raw stylus pressure  (0–1)
    QColor   color;           // foreground colour

    // Wet-media (Brush / WaterColor / Marker)
    float    blending    = 0.0f;   // colour pickup from canvas  (0–1)
    float    dilution    = 0.0f;   // water amount  (0–1)
    float    persistence = 0.5f;   // sample persistence  (0–1)
    float    blurPressure = 0.0f;  // post-blur fraction  (0–1)

    // Smudge
    float    coloring        = 0.0f;  // foreground colour injection  (0–1)
    float    uncolorPressure = 0.0f;  // erase-toward-transparent at high prs.  (0–1)

    // Blur tool
    float    blurWidth = 0.5f;        // blur kernel radius fraction  (0–1)

    // Whether alpha lock (Keep Opacity) is active — tips respect it by
    // never increasing alpha beyond what already exists in the destination.
    bool     keepOpacity = false;
};

// ─────────────────────────────────────────────────────────────────────────────
// BrushTip  –  abstract interface.  One subclass per SAI tool type.
// ─────────────────────────────────────────────────────────────────────────────
class BrushTip
{
public:
    virtual ~BrushTip() = default;

    // Paint one dab onto the painter's device (the active layer QImage).
    virtual void stamp(QPainter &p, const DabParams &dab) = 0;

    // Optional: 64×64 preview thumbnail for the preset list.
    virtual QImage preview() { return {}; }

    // Called at the start of each new stroke so stateful tips (SmudgeTip,
    // WaterColorTip) can reset their per-stroke sample buffers.
    virtual void beginStroke() {}
};

// ─────────────────────────────────────────────────────────────────────────────
// PixelTip  –  SAI "Pencil".  Hard/soft round brush, no blending.
// ─────────────────────────────────────────────────────────────────────────────
class PixelTip : public BrushTip
{
public:
    void stamp(QPainter &p, const DabParams &dab) override;
};

// ─────────────────────────────────────────────────────────────────────────────
// EraserTip  –  SAI "Eraser".  Same shape as PixelTip; composition mode
// DestinationOut is set by BrushEngine, not here.
// ─────────────────────────────────────────────────────────────────────────────
class EraserTip : public BrushTip
{
public:
    void stamp(QPainter &p, const DabParams &dab) override;
};

// ─────────────────────────────────────────────────────────────────────────────
// AirbrushTip  –  SAI "AirBrush".  Always soft edge (hardness forced to 0).
// ─────────────────────────────────────────────────────────────────────────────
class AirbrushTip : public BrushTip
{
public:
    void stamp(QPainter &p, const DabParams &dab) override;
};

// ─────────────────────────────────────────────────────────────────────────────
// BrushTipWet  –  SAI "Brush".
// Blends existing canvas colour into the paint (dab.blending) and adds
// dilution transparency (dab.dilution).  Stateful: holds a colour sample
// that is refreshed based on dab.persistence.
// ─────────────────────────────────────────────────────────────────────────────
class BrushTipWet : public BrushTip
{
public:
    void beginStroke() override { m_hasSample = false; }
    void stamp(QPainter &p, const DabParams &dab) override;

private:
    QColor m_sample;
    bool   m_hasSample = false;
};

// ─────────────────────────────────────────────────────────────────────────────
// WaterColorTip  –  SAI "Water Color".
// Like BrushTipWet but also applies a small Gaussian blur at each dab
// (dab.blurPressure scales the radius with pressure) to feather wet edges.
// ─────────────────────────────────────────────────────────────────────────────
class WaterColorTip : public BrushTip
{
public:
    void beginStroke() override { m_hasSample = false; }
    void stamp(QPainter &p, const DabParams &dab) override;

private:
    QColor m_sample;
    bool   m_hasSample = false;

    // Applies a simple box blur to a dab mask image (approximate Gaussian).
    static QImage boxBlur(const QImage &src, int radius);
};

// ─────────────────────────────────────────────────────────────────────────────
// MarkerTip  –  SAI "Marker".
// Flat opacity (no pressure→opacity mapping), blending + persistence like
// WaterColor but no dilution.
// ─────────────────────────────────────────────────────────────────────────────
class MarkerTip : public BrushTip
{
public:
    void beginStroke() override { m_hasSample = false; }
    void stamp(QPainter &p, const DabParams &dab) override;

private:
    QColor m_sample;
    bool   m_hasSample = false;
};

// ─────────────────────────────────────────────────────────────────────────────
// SmudgeTip  –  SAI "Smudge".
// Picks up canvas colour and smears it.  dab.coloring injects foreground
// colour; dab.uncolorPressure erases at high pressure.
// ─────────────────────────────────────────────────────────────────────────────
class SmudgeTip : public BrushTip
{
public:
    void beginStroke() override { m_hasSample = false; }
    void stamp(QPainter &p, const DabParams &dab) override;

private:
    QImage m_sample;
    bool   m_hasSample = false;
};

// ─────────────────────────────────────────────────────────────────────────────
// BlurTip  –  SAI "Blur".
// Reads the canvas under the dab, runs a Gaussian blur scaled by
// dab.blurWidth, and writes it back.
// ─────────────────────────────────────────────────────────────────────────────
class BlurTip : public BrushTip
{
public:
    void stamp(QPainter &p, const DabParams &dab) override;

private:
    static QImage boxBlur(const QImage &src, int radius);
};

// ─────────────────────────────────────────────────────────────────────────────
// ChalkTip  –  extra texture tip (dry grain).
// ─────────────────────────────────────────────────────────────────────────────
class ChalkTip : public BrushTip
{
public:
    void stamp(QPainter &p, const DabParams &dab) override;
};
