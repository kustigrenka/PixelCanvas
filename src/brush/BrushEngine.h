#pragma once

#include <QObject>
#include <QColor>
#include <QImage>
#include <QPainter>
#include <QVector>

#include "Stroke.h"
#include "BrushTip.h"
#include "BrushSettings.h"

class BrushEngine : public QObject
{
    Q_OBJECT

public:
    explicit BrushEngine(QObject *parent = nullptr);
    ~BrushEngine() override;

    // ── Preset management ─────────────────────────────────────────────────────
    void setPresets(const QVector<BrushPreset> &presets);
    void addPreset(const BrushPreset &preset);
    void removePreset(int index);
    void renamePreset(int index, const QString &name);
    void setActivePreset(int index);

    int                         activePresetIndex() const { return m_activePreset; }
    const QVector<BrushPreset> &presets()           const { return m_presets; }

    // ── Settings ──────────────────────────────────────────────────────────────
    void setSettings(const BrushSettings &s);
    void setColor(const QColor &c)     { m_color = c; }
    void setActiveLayer(QImage *layer) { m_layer = layer; }

    const BrushSettings &settings()    const { return m_settings; }
    QColor               color()       const { return m_color; }
    QImage              *activeLayer() const { return m_layer; }

    // Apply texture / shape BMP to the current tip (switches to TextureTip
    // if the current tip doesn't support bitmaps).
    void setTipTexture(const QString &path);
    void setTipShape(const QString &path);

    // Accessors for the current imported paths (used by the preview renderer)
    const QString &currentShapePath()   const { return m_tipShapePath; }
    const QString &currentTexturePath() const { return m_tipTexturePath; }

    // ── Stroke pipeline ───────────────────────────────────────────────────────
    void  beginStroke();
    QRect addSample(const StrokeSample &s);
    QRect endStroke();
    QRect renderStroke(const Stroke &stroke);

    // ── Painter suspend / resume ──────────────────────────────────────────────
    void suspendStrokePainter();
    void resumeStrokePainter();

signals:
    void presetsChanged();
    void activePresetChanged(int index);

private:
    QRect   stampDab(const QPointF &pos, float pressure);
    void applyTipType(const BrushSettings &s);

    // ── Preset list ───────────────────────────────────────────────────────────
    QVector<BrushPreset>  m_presets;
    int                   m_activePreset = 0;

    // ── Active settings ───────────────────────────────────────────────────────
    BrushSettings  m_settings;
    QColor         m_color  = Qt::black;
    QImage        *m_layer  = nullptr;

    BrushTip      *m_tip        = nullptr;
    TipType        m_curTipType = TipType::Pixel;

    // Remembered so TextureTip survives tip-type changes
    QString        m_tipTexturePath;
    QString        m_tipShapePath;

    // ── Persistent stroke painter ─────────────────────────────────────────────
    QPainter      *m_painter = nullptr;

    // ── Scratch layer (dry-media stroke accumulation fix) ─────────────────────
    // Dry tips (Pixel, Eraser, Chalk, Texture) draw onto this transparent image
    // during a stroke so overlapping dabs don't stack up opacity.
    // On endStroke it is composited onto m_layer once with SourceOver.
    QImage         m_strokeScratch;
    bool           m_useScratch = false;

    // Returns true for tip types that draw opaque/flat colour and must not
    // accumulate alpha across overlapping dabs within a single stroke.
    bool isDryTip() const {
        return m_settings.tipType == TipType::Pixel    ||
               m_settings.tipType == TipType::Eraser   ||
               m_settings.tipType == TipType::SelEraser ||
               m_settings.tipType == TipType::Chalk    ||
               (m_tip && dynamic_cast<TextureTip*>(m_tip) != nullptr);
    }

public:
    // Expose the scratch layer so CanvasWidget can include it in recomposite
    // during an active stroke (for correct live preview).
    const QImage  *strokeScratch()  const { return m_useScratch ? &m_strokeScratch : nullptr; }
    bool           hasScratch()     const { return m_useScratch && !m_strokeScratch.isNull(); }

private:

    // ── Per-stroke state ──────────────────────────────────────────────────────
    bool    m_inStroke     = false;
    float   m_distAccum    = 0.0f;
    float   m_lastPressure = 1.0f;
    QPointF m_lastRaw;
    QPointF m_lastSmoothed;

    int     m_crCount = 0;   // sample counter, reset each stroke

    QPainter::CompositionMode m_compMode = QPainter::CompositionMode_SourceOver;
};
