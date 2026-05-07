#pragma once

#include <QObject>
#include <QColor>
#include <QImage>
#include <QPainter>

#include "Stroke.h"
#include "BrushTip.h"
#include "BrushSettings.h"   // ← moved out; include separately for UI-only consumers

// ─────────────────────────────────────────────────────────────────────────────
// BrushEngine  (Phase 4)
//
// Turns a completed (or in-progress) Stroke into dabs on a Layer's QImage.
//
// Workflow:
//   1. CanvasWidget calls beginStroke()  before the first sample.
//   2. CanvasWidget calls addSample()    on every tablet/mouse move event.
//   3. CanvasWidget calls endStroke()    on release.
//
// The engine accumulates a distance counter and stamps a dab whenever the
// cursor has moved far enough (spacing * diameter pixels).
//
// Catmull-Rom interpolation is applied over the last 4 points so strokes
// follow curves smoothly even at low input rates.
// ─────────────────────────────────────────────────────────────────────────────
class BrushEngine : public QObject
{
    Q_OBJECT

public:
    explicit BrushEngine(QObject *parent = nullptr);
    ~BrushEngine() override;

    // ── Configuration ─────────────────────────────────────────────────────────
    void setSettings(const BrushSettings &s);
    void setColor(const QColor &c)          { m_color = c; }
    void setActiveLayer(QImage *layer)      { m_layer = layer; }

    const BrushSettings &settings() const  { return m_settings; }
    QColor               color()    const  { return m_color; }

    // ── Stroke pipeline ───────────────────────────────────────────────────────

    // Call before the first sample of a new stroke.
    void beginStroke();

    // Call for each new input sample. Returns the dirty rect that was painted.
    QRect addSample(const StrokeSample &s);

    // Call on pointer release.
    QRect endStroke();

    // ── Direct render (whole stroke at once, for redo replay) ─────────────────
    QRect renderStroke(const Stroke &stroke);

private:
    // Stamp a single dab at `pos` with `pressure` and return its bounding rect.
    QRect stampDab(const QPointF &pos, float pressure);

    // Catmull-Rom interpolation between p1 and p2 given context p0 and p3.
    static QPointF catmullRom(const QPointF &p0, const QPointF &p1,
                               const QPointF &p2, const QPointF &p3, float t);

    // ── State ──────────────────────────────────────────────────────────────────
    BrushSettings  m_settings;
    QColor         m_color   = Qt::black;
    QImage        *m_layer   = nullptr;    // non-owning pointer to active layer pixels

    BrushTip      *m_tip     = nullptr;    // owned; recreated when tip type changes

    // Per-stroke state
    bool           m_inStroke      = false;
    float          m_distAccum     = 0.0f; // accumulated distance since last dab
    float          m_lastPressure  = 1.0f;
    QPointF        m_lastRaw;              // last raw position (before smoothing)
    QPointF        m_lastSmoothed;         // last smoothed position

    // Ring buffer of last 4 smoothed positions for Catmull-Rom
    static constexpr int kCRBuf = 4;
    QPointF  m_crBuf[kCRBuf];
    int      m_crCount = 0;               // how many valid samples in crBuf

    QPainter::CompositionMode m_compMode = QPainter::CompositionMode_SourceOver;
};
