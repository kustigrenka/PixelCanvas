#pragma once

#include <QWidget>
#include <QColor>

// ─────────────────────────────────────────────────────────────────────────────
// BrushPreviewWidget  –  live stroke preview bar shown in the Preview dock.
//
// Extracted from ToolbarPanel.cpp so it can be forward-declared, unit-tested,
// or reused independently.
//
// Call updatePreview() whenever brush settings or the active color change.
// ─────────────────────────────────────────────────────────────────────────────
class BrushPreviewWidget : public QWidget
{
    Q_OBJECT

public:
    explicit BrushPreviewWidget(QWidget *parent = nullptr);

    void updatePreview(const QColor &color, float size, float opacity, float hardness);

protected:
    void paintEvent(QPaintEvent *) override;

private:
    QColor m_color    = Qt::black;
    float  m_size     = 10.0f;
    float  m_opacity  = 1.0f;
    float  m_hardness = 0.8f;
};
