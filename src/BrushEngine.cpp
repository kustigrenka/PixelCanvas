#include "BrushEngine.h"

#include <QPainter>
#include <cmath>
#include <algorithm>
#include <QDebug> 

// ─────────────────────────────────────────────────────────────────────────────
BrushEngine::BrushEngine(QObject *parent)
    : QObject(parent)
    , m_tip(new PixelTip)
{}

BrushEngine::~BrushEngine()
{
    delete m_tip;
}

// ─────────────────────────────────────────────────────────────────────────────
void BrushEngine::setSettings(const BrushSettings &s)
{
    const bool tipChanged = (s.tipType != m_settings.tipType);
    m_settings = s;

    if (tipChanged)
    {
        delete m_tip;
        switch (s.tipType)
        {
        case BrushSettings::TipType::Eraser:
            m_tip    = new EraserTip;
            m_compMode = QPainter::CompositionMode_DestinationOut;
            break;
        case BrushSettings::TipType::Airbrush:
            m_tip    = new AirbrushTip;
            m_compMode = QPainter::CompositionMode_SourceOver;
            break;
        case BrushSettings::TipType::Smudge:
            m_tip      = new SmudgeTip;
            m_compMode = QPainter::CompositionMode_SourceOver;
            break;
        case BrushSettings::TipType::Chalk:
            m_tip      = new ChalkTip;
            m_compMode = QPainter::CompositionMode_SourceOver;
            break;
        default:
            m_tip    = new PixelTip;
            m_compMode = QPainter::CompositionMode_SourceOver;
            break;
        }
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
}

QRect BrushEngine::addSample(const StrokeSample &s)
{
    if (!m_inStroke || !m_layer) return {};

    // ── 1. Smooth the raw position ─────────────────────────────────────────
    QPointF smoothed;
    if (m_crCount == 0)
    {
        // First sample — no history yet
        smoothed      = s.pos;
        m_lastRaw     = s.pos;
        m_lastSmoothed = s.pos;
    }
    else
    {
       smoothed = m_settings.smoothing * s.pos
             + (1.0f - m_settings.smoothing) * m_lastSmoothed;
    }

    // Push into Catmull-Rom ring buffer
    m_crBuf[m_crCount % kCRBuf] = smoothed;
    ++m_crCount;
    m_lastRaw      = s.pos;
    m_lastSmoothed = smoothed;

    // ── 2. Determine start/end segment for interpolation ───────────────────
    // We need at least 2 points to draw anything.
    if (m_crCount < 2) return {};

    const QPointF segEnd   = smoothed;
    const QPointF segStart = m_crBuf[(m_crCount - 2) % kCRBuf];

    // Segment length
    const QPointF delta  = segEnd - segStart;
    const float   segLen = std::sqrt(static_cast<float>(delta.x() * delta.x()
                                                        + delta.y() * delta.y()));
    if (segLen < 0.01f) return {};

    // ── 3. Walk the segment, stamping dabs at spacing intervals ───────────
    const float diameter = m_settings.size;
    const float spacing  = std::max(diameter * m_settings.spacing, 1.0f);

    QRect dirty;

    float walked = 0.0f;
    while (m_distAccum + (segLen - walked) >= spacing)
    {
        const float stepToNext = spacing - m_distAccum;
        walked    += stepToNext;
        m_distAccum = 0.0f;

        const float t = walked / segLen;

        QPointF dabPos;
        if (m_crCount >= 4)
        {
            // Full Catmull-Rom with 4-point context
            const QPointF &p0 = m_crBuf[(m_crCount - 4) % kCRBuf];
            const QPointF &p1 = segStart;   // always the start of current segment
            const QPointF &p2 = segEnd;     // always the end of current segment  
            const QPointF &p3 = m_crBuf[(m_crCount - 1) % kCRBuf];
            dabPos = catmullRom(p0, p1, p2, p3, std::clamp(t, 0.0f, 1.0f));
        }
        else
        {
            // Not enough history — linear interpolation
            dabPos = segStart + t * delta;
        }

        const QRect r = stampDab(dabPos, s.pressure);
        dirty = dirty.isEmpty() ? r : dirty.united(r);
    }

    m_distAccum    += segLen - walked;
    m_lastPressure  = s.pressure;
    return dirty;
}

QRect BrushEngine::endStroke()
{
    QRect r;
    if (m_inStroke && m_layer && m_crCount >= 1)
    {
        r = stampDab(m_lastSmoothed, m_lastPressure);
    }
    m_inStroke  = false;
    m_crCount   = 0;
    m_distAccum = 0.0f;
    return r;
}

// ─────────────────────────────────────────────────────────────────────────────
// Whole-stroke render  (used by redo replay — Ph. 7)
// ─────────────────────────────────────────────────────────────────────────────
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
// Stamp a single dab
// ─────────────────────────────────────────────────────────────────────────────
QRect BrushEngine::stampDab(const QPointF &pos, float pressure)
{
    if (!m_layer || !m_tip) return {};

    // ── Pressure dynamics ──────────────────────────────────────────────────
    // sizeDynamics == 0   → full size regardless of pressure
    // sizeDynamics == 1   → size scales linearly with pressure
    // Size: interpolate between minSizeFraction and 1.0 based on pressure
    const float sizeMul    = m_settings.minSizeFraction
                        + (1.0f - m_settings.minSizeFraction) * pressure;
    const float opacityMul = 1.0f - m_settings.opacityDynamics * (1.0f - pressure);
    
    qDebug() << "pressure:" << pressure << "opacityDynamics:" << m_settings.opacityDynamics << "opacityMul:" << opacityMul;
    DabParams dab;
    dab.center   = pos;
    dab.diameter = m_settings.size * sizeMul;
    dab.opacity  = m_settings.opacity * opacityMul;
    dab.hardness = m_settings.hardness;
    dab.pressure = pressure;
    dab.color    = m_color;

    // ── Paint onto layer ───────────────────────────────────────────────────
    QPainter p(m_layer);
    p.setRenderHint(QPainter::Antialiasing, false);  // we do our own AA in PixelTip
    p.setCompositionMode(m_compMode);

    m_tip->stamp(p, dab);
    p.end();

    // Return bounding rect of the dab (for dirty-rect tracking)
    const int r = static_cast<int>(std::ceil(dab.diameter * 0.5f)) + 2;
    return QRect(static_cast<int>(pos.x()) - r,
                 static_cast<int>(pos.y()) - r,
                 2 * r, 2 * r);
}

// ─────────────────────────────────────────────────────────────────────────────
// Catmull-Rom interpolation
// ─────────────────────────────────────────────────────────────────────────────
QPointF BrushEngine::catmullRom(const QPointF &p0, const QPointF &p1,
                                 const QPointF &p2, const QPointF &p3, float t)
                                 
{
    // Centripetal Catmull-Rom (alpha=0.5) — prevents cusps and loops on fast input
    auto dist = [](const QPointF &a, const QPointF &b) -> float {
        const float dx = b.x()-a.x(), dy = b.y()-a.y();
        return std::pow(dx*dx + dy*dy, 0.25f);  // sqrt then sqrt = ^0.25
    };

    const float t0 = 0.0f;
    const float t1 = t0 + dist(p0, p1);
    const float t2 = t1 + dist(p1, p2);
    const float t3 = t2 + dist(p2, p3);

    const float tc = t1 + t * (t2 - t1);  // remap t into [t1, t2]

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