#pragma once

#include <QObject>
#include <QColor>
#include <QImage>
#include <QPainter>
#include <QVector>

#include "Stroke.h"
#include "BrushTip.h"
#include "BrushSettings.h"

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
// cursor has moved far enough (spacing * effectiveDiameter pixels).
//
// Catmull-Rom interpolation is applied over the last 4 points so strokes
// follow curves smoothly even at low input rates.
//
// Preset management:
//   BrushEngine owns a flat list of BrushPresets.  The active preset index
//   drives the current BrushSettings.  ToolbarPanel reads/writes this list
//   and emits signals that call setActivePreset() / setSettings().
// ─────────────────────────────────────────────────────────────────────────────
class BrushEngine : public QObject
{
    Q_OBJECT

public:
    explicit BrushEngine(QObject *parent = nullptr);
    ~BrushEngine() override;

    // ── Preset management ─────────────────────────────────────────────────────

    // Replace the entire preset list (called once at startup from ProjectIO or
    // when the user imports a preset pack).
    void setPresets(const QVector<BrushPreset> &presets);

    // Add a new preset (user clicked "+").
    void addPreset(const BrushPreset &preset);

    // Remove preset at index.  Falls back to index 0 if active was removed.
    void removePreset(int index);

    // Rename preset at index.
    void renamePreset(int index, const QString &name);

    // Activate a preset: copies its BrushSettings to m_settings and
    // recreates m_tip if the TipType changed.
    void setActivePreset(int index);

    int                         activePresetIndex() const { return m_activePreset; }
    const QVector<BrushPreset> &presets()           const { return m_presets; }

    // ── Per-stroke settings overrides ─────────────────────────────────────────
    // ToolbarPanel sliders call these to update the *current* preset's settings
    // live (without creating a new preset).

    void setSettings(const BrushSettings &s);
    void setColor(const QColor &c)     { m_color = c; }
    void setActiveLayer(QImage *layer) { m_layer = layer; }

    const BrushSettings &settings() const { return m_settings; }
    QColor               color()    const { return m_color; }
    QImage              *activeLayer() const { return m_layer; }

    // ── Stroke pipeline ───────────────────────────────────────────────────────

    void  beginStroke();
    QRect addSample(const StrokeSample &s);
    QRect endStroke();

    // Direct render — whole stroke at once (redo replay).
    QRect renderStroke(const Stroke &stroke);

signals:
    // Emitted whenever the preset list changes (add / remove / rename).
    void presetsChanged();

    // Emitted when the active preset index changes.
    void activePresetChanged(int index);

private:
    QRect   stampDab(const QPointF &pos, float pressure);

    static QPointF catmullRom(const QPointF &p0, const QPointF &p1,
                               const QPointF &p2, const QPointF &p3, float t);

    // Recreates m_tip and m_compMode to match s.tipType.
    void applyTipType(const BrushSettings &s);

    // ── Preset list ───────────────────────────────────────────────────────────
    QVector<BrushPreset>  m_presets;
    int                   m_activePreset = 0;

    // ── Active settings (copy of m_presets[m_activePreset].settings) ─────────
    BrushSettings  m_settings;
    QColor         m_color   = Qt::black;
    QImage        *m_layer   = nullptr;

    BrushTip      *m_tip     = nullptr;   // owned; recreated on tip-type change
    TipType        m_curTipType = TipType::Pixel;

    // ── Per-stroke state ──────────────────────────────────────────────────────
    bool    m_inStroke     = false;
    float   m_distAccum    = 0.0f;
    float   m_lastPressure = 1.0f;
    QPointF m_lastRaw;
    QPointF m_lastSmoothed;

    static constexpr int kCRBuf = 4;
    QPointF m_crBuf[kCRBuf];
    int     m_crCount = 0;

    QPainter::CompositionMode m_compMode = QPainter::CompositionMode_SourceOver;
};
