#pragma once

#include <QWidget>
#include <QColor>
#include <QImage>

// ─────────────────────────────────────────────────────────────────────────────
// ColorWheelWidget  –  inline SAI2-style color picker  (Member 4, Ph.5)
//
// Layout:
//   • Outer ring  : hue  (click / drag to change hue)
//   • Inner square: saturation (X) × value (Y)  (click / drag)
//
// Emits colorChanged(QColor) whenever the selection moves.
// Call setColor() to set programmatically (e.g. when loading a project).
// ─────────────────────────────────────────────────────────────────────────────
class ColorWheelWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ColorWheelWidget(QWidget *parent = nullptr);

    QColor color() const { return m_color; }

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
    enum class DragZone { None, Ring, Square };

    void rebuildCache();          // regenerate wheel + square images
    void handleMouse(QPointF pos);
    void pickFromRing(QPointF pos);
    void pickFromSquare(QPointF pos);

    // Geometry helpers (all depend on current widget size)
    QPointF center() const;
    float   outerRadius() const;
    float   innerRadius() const;
    QRectF  squareRect() const;

    // ── State ──────────────────────────────────────────────────────────────────
    QColor   m_color   = QColor::fromHsvF(0.0f, 1.0f, 1.0f);  // red default
    float    m_hue     = 0.0f;   // 0–1
    float    m_sat     = 1.0f;   // 0–1
    float    m_val     = 1.0f;   // 0–1

    DragZone m_drag    = DragZone::None;

    // Cached bitmaps (rebuilt on resize or hue change)
    QImage   m_wheelImg;   // full widget size, ring only (rest transparent)
    QImage   m_squareImg;  // SV square for current hue
    bool     m_cacheDirty  = true;
    bool     m_squareDirty = true;
};
