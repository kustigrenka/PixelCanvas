#include "BrushEngine.h"

#include <QPainter>
#include <cmath>
#include <algorithm>

// ─────────────────────────────────────────────────────────────────────────────
// Construction / destruction
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
    m_tip        = nullptr;
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

    // Re-apply any imported texture / shape paths to the newly created tip.
    if (!m_tipTexturePath.isEmpty() || !m_tipShapePath.isEmpty())
    {
        auto *tt = dynamic_cast<TextureTip *>(m_tip);
        if (!tt)
        {
            // Upgrade to TextureTip, preserving the composition mode.
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

    // If both paths are now empty, restore the plain tip.
    if (path.isEmpty() && m_tipShapePath.isEmpty())
    {
        applyTipType(m_settings);
        return;
    }

    auto *tt = dynamic_cast<TextureTip *>(m_tip);
    if (!tt)
    {
        delete m_tip;
        m_tip      = tt = new TextureTip(path, m_tipShapePath);
        m_compMode = QPainter::CompositionMode_SourceOver;
    }
    else
    {
        tt->setTexture(path);
    }
}

void BrushEngine::setTipShape(const QString &path)
{
    if (path == m_tipShapePath) return;

    m_tipShapePath = path;
    m_shapeScaled  = QImage();
    m_shapeScaledD = -1;

    const bool isDryTipType = (m_settings.tipType == TipType::Pixel
                            || m_settings.tipType == TipType::Chalk
                            || m_settings.tipType == TipType::Eraser
                            || m_settings.tipType == TipType::SelEraser);

    if (isDryTipType)
    {
        if (!path.isEmpty())
        {
            // Upgrade to TextureTip to use the BMP shape.
            auto *tt = dynamic_cast<TextureTip *>(m_tip);
            if (!tt)
            {
                delete m_tip;
                m_tip      = tt = new TextureTip(m_tipTexturePath, path);
                m_compMode = QPainter::CompositionMode_SourceOver;
            }
            else
            {
                tt->setShape(path);
            }
        }
        else
        {
            // Empty shape path — restore plain tip, keep texture if non-empty.
            applyTipType(m_settings);
            if (!m_tipTexturePath.isEmpty())
            {
                auto *tt = dynamic_cast<TextureTip *>(m_tip);
                if (!tt)
                {
                    delete m_tip;
                    m_tip      = tt = new TextureTip(m_tipTexturePath, {});
                    m_compMode = QPainter::CompositionMode_SourceOver;
                }
                else
                {
                    tt->setTexture(m_tipTexturePath);
                }
            }
        }
    }
    else
    {
        // Wet tips — use the shape as a mask in stampDab().
        m_shapeMask = path.isEmpty() ? QImage()
                                     : TextureTip::loadGrayscaleMaskPublic(path);
        if (dynamic_cast<TextureTip *>(m_tip))
            applyTipType(m_settings);
    }

    // Auto-adjust spacing based on ink coverage of the imported BMP.
    if (!path.isEmpty())
    {
        float newSpacing = 0.03f;
        QImage bmp(path);
        if (!bmp.isNull())
        {
            const QImage gray  = bmp.convertToFormat(QImage::Format_Grayscale8);
            int ink   = 0;
            int total = gray.width() * gray.height();
            for (int y = 0; y < gray.height(); ++y)
            {
                const uchar *row = gray.constScanLine(y);
                for (int x = 0; x < gray.width(); ++x)
                    if (row[x] < 128) ++ink;
            }
            const float cov = total > 0 ? float(ink) / float(total) : 0.f;
            if      (cov < 0.05f) newSpacing = 0.01f;
            else if (cov < 0.15f) newSpacing = 0.02f;
            else                  newSpacing = 0.03f;
        }
        m_settings.spacing = newSpacing;
        if (m_activePreset >= 0 && m_activePreset < m_presets.size())
            m_presets[m_activePreset].settings.spacing = newSpacing;
    }
    else
    {
        m_settings.spacing = 0.08f;
        if (m_activePreset >= 0 && m_activePreset < m_presets.size())
            m_presets[m_activePreset].settings.spacing = 0.08f;
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

    // Safety: ensure the tip is never null.
    if (!m_tip)
        applyTipType(m_settings);

    if (m_tip)
        m_tip->beginStroke();

    if (!m_layer) return;

    // Dry-media tips (Pixel, Eraser, Chalk, Texture) render onto a transparent
    // scratch image so overlapping dabs within one stroke never accumulate
    // opacity (the classic "tube / halo" artefact).  On endStroke() the scratch
    // is composited onto the real layer exactly once.
    m_useScratch = isDryTip();

    delete m_painter;
    if (m_useScratch)
    {
        m_strokeScratch = QImage(m_layer->size(), QImage::Format_ARGB32_Premultiplied);
        m_strokeScratch.fill(Qt::transparent);
        m_painter = new QPainter(&m_strokeScratch);
    }
    else
    {
        m_painter = new QPainter(m_layer);
    }
    m_painter->setRenderHint(QPainter::Antialiasing, false);
}

QRect BrushEngine::addSample(const StrokeSample &s)
{
    if (!m_inStroke || !m_layer) return {};

    // ── 1. Input smoothing (EMA stabiliser) ───────────────────────────────────
    //
    // smoothing = 0.0 → raw input (no lag)
    // smoothing = 0.9 → heavy SAI-style stabilisation
    QPointF smoothed;
    if (m_crCount == 0)
    {
        smoothed       = s.pos;
        m_lastRaw      = s.pos;
        m_lastSmoothed = s.pos;
    }
    else
    {
        const float lag = m_settings.smoothing;
        smoothed = (1.0f - lag) * s.pos + lag * m_lastSmoothed;
    }

    const QPointF prev = m_lastSmoothed;
    m_crCount++;
    m_lastRaw      = s.pos;
    m_lastSmoothed = smoothed;

    // ── 2. First sample: stamp a dot and return ───────────────────────────────
    if (m_crCount == 1)
        return stampDab(smoothed, s.pressure);

    // ── 3. Walk prev → smoothed, placing dabs at fixed spacing ───────────────
    //
    // Linear interpolation between consecutive smoothed points is sufficient.
    // With a tablet generating 60–200 Hz, points are already close together;
    // the EMA above shapes the curve correctly without any spline.
    // Catmull-Rom is intentionally omitted: at high input rates it overshoots
    // and produces the visible lateral oscillations (parallel wavy-lines artefact).
    const QPointF delta  = smoothed - prev;
    const float   segLen = std::sqrt(static_cast<float>(
                               delta.x() * delta.x() + delta.y() * delta.y()));
    if (segLen < 0.01f) return {};

    const float diameter = m_settings.effectiveDiameter();
    const float spacing  = std::max(diameter * m_settings.spacing, 1.0f);

    QRect dirty;
    float walked = 0.0f;

    while (m_distAccum + (segLen - walked) >= spacing)
    {
        const float   step   = spacing - m_distAccum;
        walked      += step;
        m_distAccum  = 0.0f;

        const float   t      = walked / segLen;
        const QPointF dabPos = prev + t * delta;

        const QRect r = stampDab(dabPos, s.pressure);
        dirty = dirty.isEmpty() ? r : dirty.united(r);
    }

    m_distAccum   += segLen - walked;
    m_lastPressure = s.pressure;
    return dirty;
}

QRect BrushEngine::endStroke()
{
    QRect dirty;

    // Stamp the final position to avoid a missing dot at the stroke tail.
    if (m_inStroke && m_layer && m_crCount >= 1)
    {
        const QRect r = stampDab(m_lastSmoothed, m_lastPressure);
        dirty = dirty.isEmpty() ? r : dirty.united(r);
    }

    // Close the scratch painter before compositing.
    delete m_painter;
    m_painter = nullptr;

    // Composite scratch → real layer (dry media only, once per stroke).
    if (m_useScratch && m_layer && !m_strokeScratch.isNull())
    {
        QPainter lp(m_layer);

        if (m_settings.blendMode == BrushBlendMode::Erase
         || m_settings.tipType  == TipType::Eraser
         || m_settings.tipType  == TipType::SelEraser)
        {
            // No setOpacity() here — it breaks DestinationOut on premultiplied
            // images.  Opacity is already baked into dab alpha via
            // opacityAtPressure().
            lp.setCompositionMode(QPainter::CompositionMode_DestinationOut);
        }
        else
        {
            lp.setCompositionMode(QPainter::CompositionMode_SourceOver);
            lp.setOpacity(static_cast<double>(m_settings.opacity));
        }

        lp.drawImage(0, 0, m_strokeScratch);
        m_strokeScratch = QImage();
    }

    m_useScratch = false;
    m_inStroke   = false;
    m_crCount    = 0;
    m_distAccum  = 0.0f;
    return dirty;
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
        // Resume onto scratch (dry tip) or layer (wet tip) as appropriate.
        QPaintDevice *target = m_useScratch
            ? static_cast<QPaintDevice *>(&m_strokeScratch)
            : static_cast<QPaintDevice *>(m_layer);
        m_painter->begin(target);
        m_painter->setRenderHint(QPainter::Antialiasing, false);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// stampDab  –  render a single dab at the given position / pressure
// ─────────────────────────────────────────────────────────────────────────────

QRect BrushEngine::stampDab(const QPointF &pos, float pressure)
{
    if (!m_layer || !m_tip || !m_painter) return {};

    const bool isEraser = m_settings.tipType  == TipType::Eraser
                       || m_settings.tipType  == TipType::SelEraser
                       || m_settings.blendMode == BrushBlendMode::Erase;

    // ── Build DabParams ───────────────────────────────────────────────────────
    DabParams dab;
    dab.center          = pos;
    dab.diameter        = m_settings.sizeAtPressure(pressure);
    dab.opacity         = (m_useScratch && !isEraser) ? 1.0f
                                                      : m_settings.opacityAtPressure(pressure);
    dab.hardness        = m_settings.hardness * (0.3f + 0.7f * pressure);
    dab.pressure        = pressure;
    dab.color           = m_color;
    dab.blending        = m_settings.blending        * (1.0f - pressure);
    dab.dilution        = m_settings.dilution        * (1.0f - pressure);
    dab.persistence     = m_settings.persistence     * (0.3f  + 0.7f * pressure);
    dab.blurPressure    = m_settings.blurPressure;
    dab.coloring        = m_settings.coloring        * pressure;
    dab.uncolorPressure = m_settings.uncolorPressure;
    dab.blurWidth       = m_settings.blurWidth       * (0.2f  + 0.8f * pressure);
    dab.keepOpacity     = m_settings.keepOpacity;
    dab.brushShape      = m_settings.brushShape;
    dab.textureStrength = m_settings.textureStrength;
    dab.shapeMask       = m_shapeMask.isNull() ? nullptr : &m_shapeMask;

    // ── Resolve composition mode ──────────────────────────────────────────────
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

    // Dry tips always use SourceOver onto the scratch layer; the final blend
    // mode is applied once during endStroke() when compositing to m_layer.
    m_painter->setCompositionMode(
        m_useScratch ? QPainter::CompositionMode_SourceOver : compMode);

    m_tip->stamp(*m_painter, dab);

    // Return the axis-aligned bounding box of the dab for dirty-rect tracking.
    const int pad = static_cast<int>(std::ceil(dab.diameter * 0.5f)) + 2;
    return QRect(static_cast<int>(pos.x()) - pad,
                 static_cast<int>(pos.y()) - pad,
                 2 * pad, 2 * pad);
}
