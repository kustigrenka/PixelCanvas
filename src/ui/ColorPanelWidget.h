#pragma once

#include <QWidget>
#include <QColor>

class ColorWheelWidget;
class ColorSlidersWidget;
class ColorSwatchWidget;
class ScratchPadWidget;
class BrushEngine;

// ─────────────────────────────────────────────────────────────────────────────
// ColorPanelWidget  –  accordion container for all colour sub-panels
// ─────────────────────────────────────────────────────────────────────────────

class ColorPanelWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ColorPanelWidget(QWidget *parent = nullptr);

    QColor color() const;
    void   setBrushEngine(BrushEngine *engine);

    ColorSwatchWidget *swatchWidget()  const { return m_swatches; }
    ScratchPadWidget  *scratchWidget() const { return m_scratch;  }

public slots:
    void setColor(const QColor &c);

signals:
    void colorChanged(const QColor &color);

private:
    void syncColor(const QColor &c, QObject *source);

    ColorWheelWidget   *m_wheel    = nullptr;
    ColorSlidersWidget *m_sliders  = nullptr;
    ColorSwatchWidget  *m_swatches = nullptr;
    ScratchPadWidget   *m_scratch  = nullptr;
};
