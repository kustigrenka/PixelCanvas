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
    static  QPointF catmullRom(const QPointF &p0, const QPointF &p1,
                                const QPointF &p2, const QPointF &p3, float t);
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
