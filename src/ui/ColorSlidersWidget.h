#pragma once

#include <QWidget>
#include <QColor>
#include <QImage>

// ─────────────────────────────────────────────────────────────────────────────
// ColorSlidersWidget  –  RGB + HSV slider view  (Member 4, Ph. 5)
//
// Shows six gradient-track sliders: R, G, B then H, S, V.
// Each track is rendered as a gradient image so the user can preview how
// dragging will affect the colour.  A thin white cursor line follows the
// current value.
//
// Emits colorChanged(QColor) on every change.
// Call setColor() to sync from an external source.
// ─────────────────────────────────────────────────────────────────────────────

class ColorSlidersWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ColorSlidersWidget(QWidget *parent = nullptr);

    QColor color() const { return m_color; }

    QSize sizeHint()        const override;
    QSize minimumSizeHint() const override { return sizeHint(); }

public slots:
    void setColor(const QColor &c);

signals:
    void colorChanged(const QColor &color);

protected:
    void paintEvent(QPaintEvent *)        override;
    void mousePressEvent(QMouseEvent *e)  override;
    void mouseMoveEvent(QMouseEvent *e)   override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void resizeEvent(QResizeEvent *)      override;

private:
    struct Slider
    {
        QString label;         // "R", "G", "B", "H", "S", "V"
        float   value  = 0.f; // 0–1
        QImage  track;         // gradient image (rebuilt on resize / colour change)
        bool    dirty  = true;
        QRect   trackRect;     // set during paint layout
    };

    void rebuildTrack(int i);
    void rebuildAllTracks();
    void syncFromColor();                    // push m_color into all slider values
    void applySlider(int idx, float t);      // write value and recolour

    // Returns the slider index under pos and the fractional x within that
    // track.  Returns {-1, 0} if no slider is hit.
    std::pair<int, float> hitTest(QPoint pos) const;

    QColor m_color = QColor::fromHsvF(0.0, 1.0, 1.0);

    // Sliders: 0=R, 1=G, 2=B, 3=H, 4=S, 5=V
    Slider m_sliders[6];

    int m_activeSlider = -1;

    // ── Layout constants ──────────────────────────────────────────────────────
    static constexpr int kLabelW = 16;   // px for the label ("R", "G", …)
    static constexpr int kValW   = 30;   // px for the numeric readout
    static constexpr int kTrackH = 14;   // slider track height
    static constexpr int kRowGap =  6;   // gap between rows
    static constexpr int kTopPad = 10;   // padding above the first row
    static constexpr int kSepGap =  8;   // extra gap between RGB and HSV groups
};
