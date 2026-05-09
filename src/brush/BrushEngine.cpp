#include "BrushEngine.h"

#include <QPainter>
#include <cmath>
#include <algorithm>

// ─────────────────────────────────────────────────────────────────────────────
BrushEngine::BrushEngine(QObject *parent)
    : QObject(parent)
{
    m_presets << BrushPreset::makePixelPencil()
              << BrushPreset::makeAirbrush()
              << BrushPreset::makeBrush()
              << BrushPreset::makeWaterColor()
              << BrushPreset::makeEraser()
              << BrushPreset::makeSmudge()
              << BrushPreset::makeBlur();

    setActivePreset(0);
}

BrushEngine::~BrushEngine()
{
    delete m_painter;
    delete m_tip;
}

// ─────────────────────────────────────────────────────────────────────────────
// Preset management
// ─────────────────────────────────────────────────────────────────────────────
void BrushEngine::setPresets(const QVector<BrushPreset> &presets)
{
    m_presets = presets;
    if (m_presets.isEmpty())
        m_presets << BrushPreset::makePixelPencil();
    m_activePreset = 0;
    setActivePreset(0);
    emit presetsChanged();
}

void BrushEngine::addPreset(const BrushPreset &preset)
{
    m_presets.append(preset);
    emit presetsChanged();
}

void BrushEngine::removePreset(int index)
{
    if (index < 0 || index >= m_presets.size()) return;
    m_presets.removeAt(index);
    if (m_presets.isEmpty())
        m_presets << BrushPreset::makePixelPencil();
    const int newActive = std::clamp(m_activePreset, 0, (int)m_presets.size() - 1);
    setActivePreset(newActive);
    emit presetsChanged();
}

void BrushEngine::renamePreset(int index, const QString &name)
{
    if (index < 0 || index >= m_presets.size()) return;
    m_presets[index].name = name;
    emit presetsChanged();
}

void BrushEngine::setActivePreset(int index)
{
    if (index < 0 || index >= m_presets.size()) return;
    m_activePreset = index;
    setSettings(m_presets[index].settings);
    emit activePresetChanged(index);
}

// ─────────────────────────────────────────────────────────────────────────────
// Settings
// ─────────────────────────────────────────────────────────────────────────────
void BrushEngine::setSettings(const BrushSettings &s)
{
    const bool tipChanged = (s.tipType != m_settings.tipType);
    m_settings = s;

    if (m_activePreset >= 0 && m_activePreset < m_presets.size())
        m_presets[m_activePreset].settings = s;

    if (tipChanged)
        applyTipType(s);
}

void BrushEngine::applyTipType(const BrushSettings &s)
{
    delete m_tip;
    m_tip = nullptr;
    m_curTipType = s.tipType;

    switch (s.tipType)
    {
    case TipType::Eraser:
    case TipType::SelEraser:
        m_tip      = new EraserTip;
        m_compMode = QPainter::CompositionMode_DestinationOut;
        break;
    case TipType::Airbrush:
        m_tip      = new AirbrushTip;
        m_compMode = QPainter::CompositionMode_SourceOver;
        break;
    case TipType::Brush:
        m_tip      = new BrushTipWet;
        m_compMode = QPainter::CompositionMode_SourceOver;
        break;
    case TipType::WaterColor:
        m_tip      = new WaterColorTip;
        m_compMode = QPainter::CompositionMode_SourceOver;
        break;
    case TipType::Marker:
        m_tip      = new MarkerTip;
        m_compMode = QPainter::CompositionMode_SourceOver;
        break;
    case TipType::Smudge:
        m_tip      = new SmudgeTip;
        m_compMode = QPainter::CompositionMode_SourceOver;
        break;
    case TipType::Blur:
        m_tip      = new BlurTip;
        m_compMode = QPainter::CompositionMode_SourceOver;
        break;
    case TipType::Chalk:
        m_tip      = new ChalkTip;
        m_compMode = QPainter::CompositionMode_SourceOver;
        break;
    case TipType::Pixel:
    case TipType::SelPen:
    default:
        m_tip      = new PixelTip;
        m_compMode = QPainter::CompositionMode_SourceOver;
        break;
    }

    // Re-apply any imported texture/shape paths to the new tip
    if (!m_tipTexturePath.isEmpty() || !m_tipShapePath.isEmpty())
    {
        auto *tt = dynamic_cast<TextureTip *>(m_tip);
        if (!tt)
        {
            // Upgrade to TextureTip, preserving composition mode
            delete m_tip;
            m_tip = tt = new TextureTip(m_tipTexturePath, m_tipShapePath);
        }
        else
        {
            if (!m_tipTexturePath.isEmpty()) tt->setTexture(m_tipTexturePath);
            if (!m_tipShapePath.isEmpty())   tt->setShape(m_tipShapePath);
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Texture / shape import
// ─────────────────────────────────────────────────────────────────────────────
void BrushEngine::setTipTexture(const QString &path)
{
    m_tipTexturePath = path;
    auto *tt = dynamic_cast<TextureTip *>(m_tip);
    if (!tt)
    {
        delete m_tip;
        m_tip = tt = new TextureTip(path, m_tipShapePath);
        m_compMode = QPainter::CompositionMode_SourceOver;
    }
    else
    {
        tt->setTexture(path);
    }
}

void BrushEngine::setTipShape(const QString &path)
{
    m_tipShapePath = path;
    auto *tt = dynamic_cast<TextureTip *>(m_tip);
    if (!tt)
    {
        delete m_tip;
        m_tip = tt = new TextureTip(m_tipTexturePath, path);
        m_compMode = QPainter::CompositionMode_SourceOver;
    }
    else
    {
        tt->setShape(path);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Stroke pipeline
// ─────────────────────────────────────────────────────────────────────────────
void BrushEngine::beginStroke()
{
    m_inStroke     = true;
    m_distAccum    = 0.0f;
    m_lastPressure = 1.0f;
    m_crCount      = 0;

    // Safety: ensure tip is never null
    if (!m_tip)
        applyTipType(m_settings);

    if (m_tip)
        m_tip->beginStroke();

    // Open the painter once for the whole stroke.
    // m_layer must be set via setActiveLayer() before beginStroke().
    if (m_layer)
    {
        delete m_painter;
        m_painter = new QPainter(m_layer);
        m_painter->setRenderHint(QPainter::Antialiasing, false);
    }
}

QRect BrushEngine::addSample(const StrokeSample &s)
{
    if (!m_inStroke || !m_layer) return {};

    // ── 1. Smooth position ────────────────────────────────────────────────
    QPointF smoothed;
    if (m_crCount == 0)
    {
        smoothed       = s.pos;
        m_lastRaw      = s.pos;
        m_lastSmoothed = s.pos;
    }
    else
    {
        smoothed = m_settings.smoothing * s.pos
                 + (1.0f - m_settings.smoothing) * m_lastSmoothed;
    }

    m_crBuf[m_crCount % kCRBuf] = smoothed;
    ++m_crCount;
    m_lastRaw      = s.pos;
    m_lastSmoothed = smoothed;

    // ── Single-click / first-sample fix ───────────────────────────────────
    // Always stamp on the very first sample so a tap with no drag draws.
    if (m_crCount < 2)
        return stampDab(smoothed, s.pressure);

    // ── 2. Walk the segment ───────────────────────────────────────────────
    const QPointF segEnd   = smoothed;
    const QPointF segStart = m_crBuf[(m_crCount - 2) % kCRBuf];
    const QPointF delta    = segEnd - segStart;
    const float   segLen   = std::sqrt(static_cast<float>(
                                 delta.x() * delta.x() + delta.y() * delta.y()));
    if (segLen < 0.01f) return {};

    const float diameter = m_settings.effectiveDiameter();
    const float spacing  = std::max(diameter * m_settings.spacing, 1.0f);

    QRect dirty;
    float walked = 0.0f;

    while (m_distAccum + (segLen - walked) >= spacing)
    {
        const float stepToNext = spacing - m_distAccum;
        walked     += stepToNext;
        m_distAccum = 0.0f;

        const float t = walked / segLen;

        QPointF dabPos;
        if (m_crCount >= 4)
        {
            const QPointF &p0 = m_crBuf[(m_crCount - 4) % kCRBuf];
            const QPointF &p1 = segStart;
            const QPointF &p2 = segEnd;
            const QPointF &p3 = m_crBuf[(m_crCount - 1) % kCRBuf];
            dabPos = catmullRom(p0, p1, p2, p3, std::clamp(t, 0.0f, 1.0f));
        }
        else
        {
            dabPos = segStart + t * delta;
        }

        const QRect r = stampDab(dabPos, s.pressure);
        dirty = dirty.isEmpty() ? r : dirty.united(r);
    }

    m_distAccum   += segLen - walked;
    m_lastPressure = s.pressure;
    return dirty;
}

QRect BrushEngine::endStroke()
{
    QRect r;
    if (m_inStroke && m_layer && m_crCount >= 1)
        r = stampDab(m_lastSmoothed, m_lastPressure);

    delete m_painter;
    m_painter = nullptr;

    m_inStroke  = false;
    m_crCount   = 0;
    m_distAccum = 0.0f;
    return r;
}

QRect BrushEngine::renderStroke(const Stroke &stroke)
{
    QRect dirty;
    beginStroke();
    for (const auto &sample : stroke)
    {
        const QRect r = addSample(sample);
        if (!r.isEmpty())
            dirty = dirty.isEmpty() ? r : dirty.united(r);
    }
    endStroke();
    return dirty;
}

// ─────────────────────────────────────────────────────────────────────────────
// Painter suspend / resume
// ─────────────────────────────────────────────────────────────────────────────
void BrushEngine::suspendStrokePainter()
{
    if (m_painter && m_painter->isActive())
        m_painter->end();
}

void BrushEngine::resumeStrokePainter()
{
    if (!m_inStroke || !m_layer) return;
    if (!m_painter) m_painter = new QPainter;
    if (!m_painter->isActive())
    {
        m_painter->begin(m_layer);
        m_painter->setRenderHint(QPainter::Antialiasing, false);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// stampDab
// ─────────────────────────────────────────────────────────────────────────────
QRect BrushEngine::stampDab(const QPointF &pos, float pressure)
{
    if (!m_layer || !m_tip || !m_painter) return {};

    DabParams dab;
    dab.center          = pos;
    dab.diameter        = m_settings.sizeAtPressure(pressure);
    dab.opacity         = m_settings.opacityAtPressure(pressure);
    dab.hardness        = m_settings.hardness;
    dab.pressure        = pressure;
    dab.color           = m_color;
    dab.blending        = m_settings.blending;
    dab.dilution        = m_settings.dilution;
    dab.persistence     = m_settings.persistence;
    dab.blurPressure    = m_settings.blurPressure;
    dab.coloring        = m_settings.coloring;
    dab.uncolorPressure = m_settings.uncolorPressure;
    dab.blurWidth       = m_settings.blurWidth;
    dab.keepOpacity     = m_settings.keepOpacity;

    QPainter::CompositionMode compMode = m_compMode;
    switch (m_settings.blendMode)
    {
    case BrushBlendMode::Multiply:   compMode = QPainter::CompositionMode_Multiply;      break;
    case BrushBlendMode::Screen:     compMode = QPainter::CompositionMode_Screen;         break;
    case BrushBlendMode::Overlay:    compMode = QPainter::CompositionMode_Overlay;        break;
    case BrushBlendMode::Luminosity: compMode = QPainter::CompositionMode_Lighten;        break;
    case BrushBlendMode::Shade:      compMode = QPainter::CompositionMode_Darken;         break;
    case BrushBlendMode::Erase:      compMode = QPainter::CompositionMode_DestinationOut; break;
    case BrushBlendMode::Vivid:
    case BrushBlendMode::Deep:
    case BrushBlendMode::Normal:
    default:
        compMode = QPainter::CompositionMode_SourceOver;
        break;
    }

    m_painter->setCompositionMode(compMode);
    m_tip->stamp(*m_painter, dab);

    const int pad = static_cast<int>(std::ceil(dab.diameter * 0.5f)) + 2;
    return QRect(static_cast<int>(pos.x()) - pad,
                 static_cast<int>(pos.y()) - pad,
                 2 * pad, 2 * pad);
}

// ─────────────────────────────────────────────────────────────────────────────
// Catmull-Rom
// ─────────────────────────────────────────────────────────────────────────────
QPointF BrushEngine::catmullRom(const QPointF &p0, const QPointF &p1,
                                 const QPointF &p2, const QPointF &p3, float t)
{
    auto dist = [](const QPointF &a, const QPointF &b) -> float {
        const float dx = b.x() - a.x(), dy = b.y() - a.y();
        return std::pow(dx * dx + dy * dy, 0.25f);
    };

    const float t0 = 0.0f;
    const float t1 = t0 + dist(p0, p1);
    const float t2 = t1 + dist(p1, p2);
    const float t3 = t2 + dist(p2, p3);
    const float tc = t1 + t * (t2 - t1);

    auto interp = [](QPointF a, QPointF b, float ta, float tb, float tx) -> QPointF {
        if (std::abs(tb - ta) < 1e-4f) return a;
        return a + (b - a) * ((tx - ta) / (tb - ta));
    };

    const QPointF A1 = interp(p0, p1, t0, t1, tc);
    const QPointF A2 = interp(p1, p2, t1, t2, tc);
    const QPointF A3 = interp(p2, p3, t2, t3, tc);
    const QPointF B1 = interp(A1, A2, t0, t2, tc);
    const QPointF B2 = interp(A2, A3, t1, t3, tc);
    return interp(B1, B2, t1, t2, tc);
}
