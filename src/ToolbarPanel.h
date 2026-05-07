#pragma once

#include <QObject>
#include <QColor>
#include <QPainter>

#include "BrushSettings.h"       // ← replaces the old BrushEngine.h include

class ColorWheelWidget;
class BrushPreviewWidget;        // ← forward-declare; defined in BrushPreviewWidget.h
class QListWidget;
class QSpinBox;
class QSlider;
class QComboBox;
class QToolButton;
class QLabel;
class QWidget;
class QFrame;

// ─────────────────────────────────────────────────────────────────────────────
// ToolbarPanel  –  signal hub + sub-widget factory  (Member 4, Ph.5)
//
// MainWindow calls the take*() methods to pull each section into its own dock:
//   takeColorWheel()    → ColorWheelWidget*            → "Color" dock
//   takeSwatchRow()     → QWidget* (prim/sec row)      → "Color" dock
//   takeBrushBody()     → QWidget* (tabs+list+sliders) → "Brushes" dock
//   takePreviewWidget() → BrushPreviewWidget*           → "Preview" dock
//
// After take*() the sections are reparented to whatever the caller passes.
// ToolbarPanel itself is never shown; it just owns the signal/slot wiring.
// ─────────────────────────────────────────────────────────────────────────────
class ToolbarPanel : public QObject
{
    Q_OBJECT

public:
    enum class Tool { Brush, Eraser, Eyedropper, Pan, Zoom };

    explicit ToolbarPanel(QObject *parent = nullptr);

    // Called by MainWindow to place each section into its dock
    ColorWheelWidget   *takeColorWheel();
    QWidget            *takeSwatchRow(QWidget *newParent);
    QWidget            *takeBrushBody();
    QWidget            *takePreviewWidget();

    // Keyboard shortcut API
    void selectTool(Tool t);

signals:
    void brushSettingsChanged(const BrushSettings &settings);
    void colorChanged(const QColor &color);
    void toolChanged(Tool tool);
    void blendModeChanged(QPainter::CompositionMode mode);

private slots:
    void onPrimaryColorClicked();
    void onSwapColors();
    void onSizeChanged(int v);
    void onOpacityChanged(int v);
    void onHardnessChanged(int v);
    void onBlendModeChanged(int index);
    void onBrushPresetSelected(int row);
    void onBrushTypeSelected(int typeIndex);

private:
    void ensureBuilt();
    void buildColorSection();
    void buildSwatchRow();
    void buildBrushBody();
    void buildPreviewArea();
    void updateColorSquares();
    void emitSettings();

    bool m_built = false;

    // ── Color state ───────────────────────────────────────────────────────────
    QColor  m_primary   = Qt::black;
    QColor  m_secondary = Qt::white;

    // ── Color section ─────────────────────────────────────────────────────────
    ColorWheelWidget   *m_colorWheel      = nullptr;
    QWidget            *m_swatchRow       = nullptr;
    QLabel             *m_primarySwatch   = nullptr;
    QLabel             *m_secondarySwatch = nullptr;

    // ── Brush body ────────────────────────────────────────────────────────────
    QWidget            *m_brushBody       = nullptr;
    QList<QToolButton*> m_brushTypeBtns;
    int                 m_activeBrushType = 0;
    QComboBox          *m_blendCombo      = nullptr;
    QListWidget        *m_presetList      = nullptr;
    QSlider            *m_sizeSlider      = nullptr;
    QSpinBox           *m_sizeSpin        = nullptr;
    QSlider            *m_opacitySlider   = nullptr;
    QSpinBox           *m_opacitySpin     = nullptr;
    QSlider            *m_hardnessSlider  = nullptr;
    QSpinBox           *m_hardnessSpin    = nullptr;

    // ── Preview ───────────────────────────────────────────────────────────────
    BrushPreviewWidget *m_preview         = nullptr;

    // ── Tool state ────────────────────────────────────────────────────────────
    Tool           m_activeTool  = Tool::Brush;
    BrushSettings  m_settings;
};
