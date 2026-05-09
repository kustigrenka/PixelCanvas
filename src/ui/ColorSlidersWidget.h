#pragma once

#include <QWidget>
#include <QColor>
#include <QImage>
#include <QLabel>

// ─────────────────────────────────────────────────────────────────────────────
// ColorSlidersWidget  –  RGB + HSV slider view  (Member 4, Ph.5)
//
// Shows six gradient-track sliders: R, G, B and H, S, V.
// Each slider track is rendered as a gradient image so the user can see
// how dragging will change the colour.  Clicking/dragging on the track
// picks the value; a thin white cursor line follows.
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
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void resizeEvent(QResizeEvent *) override;

private:
    // One slider track descriptor
    struct Slider {
        QString label;       // "R", "G", "B", "H", "S", "V"
        float   value;       // 0–1
        QImage  track;       // gradient image (rebuilt on resize/color change)
        bool    dirty = true;
        QRect   trackRect;   // set during paint layout
    };

    void rebuildTrack(int i);
    void rebuildAllTracks();

    // Returns which slider index contains pos, and the fractional x position
    // within that slider's track rect.  Returns {-1, 0} if none.
    std::pair<int,float> hitTest(QPoint pos) const;

    void applySlider(int idx, float t);  // write value and recolour
    void syncFromColor();                 // push m_color into all values

    QColor m_color = QColor::fromHsvF(0.0, 1.0, 1.0);

    // Sliders: 0=R,1=G,2=B,3=H,4=S,5=V
    Slider m_sliders[6];

    int    m_activeSlider = -1;

    static constexpr int kLabelW  = 16;   // px for "R" label
    static constexpr int kValW    = 30;   // px for numeric readout
    static constexpr int kTrackH  = 14;   // slider track height
    static constexpr int kRowGap  = 6;    // gap between rows
    static constexpr int kTopPad  = 10;
    static constexpr int kSepGap  = 8;    // extra gap between RGB and HSV groups
};
