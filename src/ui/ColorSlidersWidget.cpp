#include <QPainterPath>
#include "ColorSlidersWidget.h"

#include <QPainter>
#include <QMouseEvent>
#include <QLinearGradient>
#include <cmath>

// ─────────────────────────────────────────────────────────────────────────────
// Constructor
// ─────────────────────────────────────────────────────────────────────────────
ColorSlidersWidget::ColorSlidersWidget(QWidget *parent)
    : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    setMinimumSize(160, 130);

    const char* labels[] = { "R","G","B","H","S","V" };
    for (int i = 0; i < 6; ++i) {
        m_sliders[i].label = labels[i];
        m_sliders[i].dirty = true;
    }
    syncFromColor();
}

// ─────────────────────────────────────────────────────────────────────────────
// Public API
// ─────────────────────────────────────────────────────────────────────────────
void ColorSlidersWidget::setColor(const QColor &c)
{
    if (m_color == c) return;
    m_color = c;
    syncFromColor();
    rebuildAllTracks();
    update();
}

// ─────────────────────────────────────────────────────────────────────────────
// Internal helpers
// ─────────────────────────────────────────────────────────────────────────────
void ColorSlidersWidget::syncFromColor()
{
    m_sliders[0].value = static_cast<float>(m_color.redF());
    m_sliders[1].value = static_cast<float>(m_color.greenF());
    m_sliders[2].value = static_cast<float>(m_color.blueF());
    float h = static_cast<float>(m_color.hsvHueF());
    m_sliders[3].value = (h < 0.0f) ? 0.0f : h;
    m_sliders[4].value = static_cast<float>(m_color.hsvSaturationF());
    m_sliders[5].value = static_cast<float>(m_color.valueF());
    for (auto &s : m_sliders) s.dirty = true;
}

// Build gradient image for one slider
void ColorSlidersWidget::rebuildTrack(int i)
{
    Slider &s = m_sliders[i];
    const QRect &r = s.trackRect;
    if (r.width() <= 0 || r.height() <= 0) return;

    s.track = QImage(r.width(), r.height(), QImage::Format_ARGB32_Premultiplied);

    // Render gradient by sampling colours across the range 0→1
    const int W = r.width();
    const int H = r.height();
    for (int x = 0; x < W; ++x) {
        float t = x / static_cast<float>(W - 1);
        QColor col = m_color;
        switch (i) {
            case 0: col.setRedF(t); break;
            case 1: col.setGreenF(t); break;
            case 2: col.setBlueF(t); break;
            case 3: col = QColor::fromHsvF(t,
                        static_cast<double>(m_sliders[4].value),
                        static_cast<double>(m_sliders[5].value)); break;
            case 4: col = QColor::fromHsvF(
                        static_cast<double>(m_sliders[3].value),
                        t,
                        static_cast<double>(m_sliders[5].value)); break;
            case 5: col = QColor::fromHsvF(
                        static_cast<double>(m_sliders[3].value),
                        static_cast<double>(m_sliders[4].value),
                        t); break;
        }
        const QRgb rgb = col.rgb();
        for (int y = 0; y < H; ++y)
            reinterpret_cast<QRgb*>(s.track.scanLine(y))[x] = rgb;
    }
    s.dirty = false;
}

void ColorSlidersWidget::rebuildAllTracks()
{
    for (int i = 0; i < 6; ++i)
        if (m_sliders[i].dirty)
            rebuildTrack(i);
}

// ─────────────────────────────────────────────────────────────────────────────
// Layout: compute trackRect for each slider (used by both paint and hit test)
// ─────────────────────────────────────────────────────────────────────────────
// Returns the y-top of slider i given widget geometry
static int sliderTop(int i, int widgetH)
{
    Q_UNUSED(widgetH)
    constexpr int kTopPad  = 10;
    constexpr int kTrackH  = 14;
    constexpr int kRowGap  = 6;
    constexpr int kSepGap  = 8;
    int rowH = kTrackH + kRowGap;
    if (i < 3) return kTopPad + i * rowH;
    return kTopPad + 3 * rowH + kSepGap + (i - 3) * rowH;
}

// ─────────────────────────────────────────────────────────────────────────────
// Paint
// ─────────────────────────────────────────────────────────────────────────────
void ColorSlidersWidget::paintEvent(QPaintEvent *)
{
    // Lay out track rects (needed before we rebuild)
    const int W   = width();
    const int trackX = kLabelW + 4;
    const int trackW = W - trackX - kValW - 6;

    for (int i = 0; i < 6; ++i) {
        const int y = sliderTop(i, height());
        m_sliders[i].trackRect = QRect(trackX, y, trackW, kTrackH);
        if (m_sliders[i].dirty) rebuildTrack(i);
    }

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const QColor bg = palette().color(QPalette::Window);
    const QColor fg = palette().color(QPalette::WindowText);

    // SAI2-style dark background for the track area
    const QColor trackBg(30, 30, 30);
    const QColor cursorCol(Qt::white);

    QFont labelFont = p.font();
    labelFont.setPointSize(7);
    labelFont.setBold(true);
    p.setFont(labelFont);

    for (int i = 0; i < 6; ++i) {
        const Slider &s = m_sliders[i];
        const QRect  &r = s.trackRect;

        // Group separator line above HSV block
        if (i == 3) {
            p.setPen(QPen(QColor(70,70,70), 1));
            p.drawLine(0, r.top() - kSepGap / 2,
                       W, r.top() - kSepGap / 2);
        }

        // Label
        p.setPen(fg);
        p.drawText(QRect(0, r.top(), kLabelW, kTrackH),
                   Qt::AlignCenter, s.label);

        // Track background (rounded rect)
        {
            QPainterPath rp;
            rp.addRoundedRect(r.adjusted(-1,-1,1,1), 3, 3);
            p.fillPath(rp, trackBg);
        }

        // Gradient image
        if (!s.track.isNull())
            p.drawImage(r, s.track);

        // Rounded track border
        p.setPen(QPen(QColor(80,80,80), 0.5));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(r, 3, 3);

        // Cursor line
        const int cx = r.left() + static_cast<int>(s.value * (r.width() - 1));
        // shadow
        p.setPen(QPen(QColor(0,0,0,120), 2.0));
        p.drawLine(cx + 1, r.top() - 1, cx + 1, r.bottom() + 1);
        // white line
        p.setPen(QPen(cursorCol, 1.5));
        p.drawLine(cx, r.top() - 2, cx, r.bottom() + 2);

        // Numeric value
        int displayVal = 0;
        if (i < 3)      displayVal = qRound(s.value * 255.0f);
        else if (i == 3) displayVal = qRound(s.value * 360.0f);
        else             displayVal = qRound(s.value * 100.0f);

        p.setPen(fg);
        QFont vf = p.font();
        vf.setBold(false);
        p.setFont(vf);
        p.drawText(QRect(r.right() + 3, r.top(), kValW, kTrackH),
                   Qt::AlignVCenter | Qt::AlignLeft,
                   QString::number(displayVal));
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Hit test
// ─────────────────────────────────────────────────────────────────────────────
std::pair<int,float> ColorSlidersWidget::hitTest(QPoint pos) const
{
    for (int i = 0; i < 6; ++i) {
        const QRect hit = m_sliders[i].trackRect.adjusted(-4,-6,4,6);
        if (hit.contains(pos)) {
            const QRect &r = m_sliders[i].trackRect;
            float t = static_cast<float>(pos.x() - r.left()) / r.width();
            t = std::clamp(t, 0.0f, 1.0f);
            return {i, t};
        }
    }
    return {-1, 0.0f};
}

// ─────────────────────────────────────────────────────────────────────────────
// Apply value change to a slider
// ─────────────────────────────────────────────────────────────────────────────
void ColorSlidersWidget::applySlider(int idx, float t)
{
    m_sliders[idx].value = t;

    // Rebuild colour
    if (idx < 3) {
        // RGB changed → derive HSV
        m_color.setRgbF(
            static_cast<double>(m_sliders[0].value),
            static_cast<double>(m_sliders[1].value),
            static_cast<double>(m_sliders[2].value));
        // Sync HSV sliders
        float h = static_cast<float>(m_color.hsvHueF());
        m_sliders[3].value = (h < 0.0f) ? 0.0f : h;
        m_sliders[4].value = static_cast<float>(m_color.hsvSaturationF());
        m_sliders[5].value = static_cast<float>(m_color.valueF());
    } else {
        // HSV changed → derive RGB
        m_color = QColor::fromHsvF(
            static_cast<double>(m_sliders[3].value),
            static_cast<double>(m_sliders[4].value),
            static_cast<double>(m_sliders[5].value));
        m_sliders[0].value = static_cast<float>(m_color.redF());
        m_sliders[1].value = static_cast<float>(m_color.greenF());
        m_sliders[2].value = static_cast<float>(m_color.blueF());
    }

    // Mark all tracks dirty (other channels changed too)
    for (auto &s : m_sliders) s.dirty = true;
    update();
    emit colorChanged(m_color);
}

// ─────────────────────────────────────────────────────────────────────────────
// Mouse events
// ─────────────────────────────────────────────────────────────────────────────
void ColorSlidersWidget::mousePressEvent(QMouseEvent *e)
{
    if (e->button() != Qt::LeftButton) return;
    auto [idx, t] = hitTest(e->pos());
    if (idx >= 0) {
        m_activeSlider = idx;
        applySlider(idx, t);
    }
}

void ColorSlidersWidget::mouseMoveEvent(QMouseEvent *e)
{
    if (m_activeSlider < 0) return;
    const QRect &r = m_sliders[m_activeSlider].trackRect;
    float t = static_cast<float>(e->pos().x() - r.left()) / r.width();
    t = std::clamp(t, 0.0f, 1.0f);
    applySlider(m_activeSlider, t);
}

void ColorSlidersWidget::mouseReleaseEvent(QMouseEvent *)
{
    m_activeSlider = -1;
}

// ─────────────────────────────────────────────────────────────────────────────
// Resize
// ─────────────────────────────────────────────────────────────────────────────
void ColorSlidersWidget::resizeEvent(QResizeEvent *)
{
    for (auto &s : m_sliders) s.dirty = true;
}

QSize ColorSlidersWidget::sizeHint() const
{
    // 6 rows, each kTrackH tall with kRowGap below, plus kTopPad at top,
    // plus kSepGap between RGB and HSV groups, plus some bottom padding.
    const int h = kTopPad
                + 3 * (kTrackH + kRowGap)   // R, G, B
                + kSepGap
                + 3 * (kTrackH + kRowGap)   // H, S, V
                + kRowGap;                  // bottom padding
    return QSize(180, h);
}
 