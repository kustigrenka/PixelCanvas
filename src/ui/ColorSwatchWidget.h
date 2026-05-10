#pragma once

#include <QWidget>
#include <QColor>
#include <QVector>
#include <QScrollArea>

// ─────────────────────────────────────────────────────────────────────────────
// ColorSwatchWidget  –  100 slots, fixed cell size, dynamic column count.
//
// kCellW × kCellH cells, fixed forever.
// Column count = floor((width - 2*kMargin + kPad) / (kCellW + kPad)).
// Row count    = ceil(kCount / cols).
// The widget height adjusts to fit all rows — the parent accordion section
// reads sizeHint() and sets its natural height accordingly.
//
// No QScrollArea wrapper needed — the grid is always fully visible and
// reflows as the dock is resized.
// ─────────────────────────────────────────────────────────────────────────────
class ColorSwatchWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ColorSwatchWidget(QWidget *parent = nullptr);

    QColor color() const { return m_color; }

    QVector<QColor> swatches() const     { return m_swatches; }
    void setSwatches(QVector<QColor> sw) { m_swatches = std::move(sw); update(); }

    QSize sizeHint()        const override;
    void  saveSwatches();
    void  loadSwatches();
    QSize minimumSizeHint() const override { return sizeHint(); }

public slots:
    void setColor(const QColor &c) { m_color = c; update(); }

signals:
    void colorChanged(const QColor &color);

protected:
    void paintEvent(QPaintEvent *)      override;
    void resizeEvent(QResizeEvent *)    override;
    void mousePressEvent(QMouseEvent *) override;
    bool event(QEvent *)                override;

private:
    static constexpr int kCount  = 100;
    static constexpr int kCellW  = 22;
    static constexpr int kCellH  = 22;
    static constexpr int kPad    = 2;
    static constexpr int kMargin = 4;

    // Derived from current width — updated in resizeEvent
    int cols() const { return m_cols; }
    int rows() const { return m_cols > 0 ? (kCount + m_cols - 1) / m_cols : 1; }
    int gridHeight() const { return rows() * kCellH + (rows() - 1) * kPad + 2 * kMargin; }

    QRect swatchRect(int i) const;
    int   swatchAt(QPoint p) const;
    void  showContextMenu(int idx, const QPoint &globalPos);
    void  recomputeCols();


    QColor          m_color  = Qt::black;
    QVector<QColor> m_swatches;
    int             m_cols   = 4;   // updated in resizeEvent
};