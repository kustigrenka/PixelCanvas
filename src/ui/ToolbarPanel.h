#pragma once

#include <QObject>
#include <QColor>
#include <QImage>
#include <QVector>
#include <QHash>

#include "BrushSettings.h"

class ColorWheelWidget;
class BrushEngine;
class QListWidget;
class QSpinBox;
class QSlider;
class QComboBox;
class QToolButton;
class QLabel;
class QWidget;
class QFrame;
class QScrollArea;
class QGridLayout;

// ─────────────────────────────────────────────────────────────────────────────
// BrushSlot  –  one cell in the 4-column brush grid
//
// Empty slots (isEmpty() == true) display a "+" button.
// ─────────────────────────────────────────────────────────────────────────────

struct BrushSlot
{
    bool          empty       = true;
    QString       name;
    BrushSettings settings;
    QString       shapePath;     // imported BMP/PNG shape mask path
    QString       texturePath;   // imported BMP/PNG texture overlay path

    bool isEmpty() const { return empty; }
};

// ─────────────────────────────────────────────────────────────────────────────
// ToolbarPanel
// ─────────────────────────────────────────────────────────────────────────────

class ToolbarPanel : public QObject
{
    Q_OBJECT

public:
    enum class Tool { Brush, Eraser, Eyedropper, Pan, Zoom };

    // brushEngine is needed to render the live stroke preview.
    explicit ToolbarPanel(BrushEngine *brushEngine, QObject *parent = nullptr);

    ColorWheelWidget *takeColorWheel();
    QWidget          *takeSwatchRow(QWidget *newParent);
    QWidget          *takeBrushBody();

    void selectTool(Tool t);
    void onExternalColorChanged(const QColor &color);
    void saveSettings();

    static BrushSettings defaultSettings(TipType tt);

signals:
    void brushSettingsChanged(const BrushSettings &settings);
    void colorChanged(const QColor &color);
    void toolChanged(Tool tool);
    void blendModeChanged(BrushBlendMode mode);

private slots:
    void onPrimaryColorClicked();
    void onSwapColors();
    void onSizeChanged(int v);
    void onMinSizeChanged(int v);
    void onOpacityChanged(int v);
    void onMinOpacityChanged(int v);
    void onHardnessChanged(int v);
    void onBlendingChanged(int v);
    void onDilutionChanged(int v);
    void onPersistenceChanged(int v);
    void onBlurPressureChanged(int v);
    void onColoringChanged(int v);
    void onUncolorPressureChanged(int v);
    void onBlurWidthChanged(int v);
    void onBlendModeChanged(int index);
    void onShapeChanged(int shape) {}   // 0=Circle 1=Diamond 2=Square

private:
    // ── Build helpers ─────────────────────────────────────────────────────────
    void ensureBuilt();
    void buildColorSection();
    void buildSwatchRow();
    void buildBrushBody();

    // ── Grid helpers ──────────────────────────────────────────────────────────
    void rebuildGrid();
    void selectSlot(int index);
    void addCustomBrush(int slotIndex);   // opens dialog, fills slot
    void removeSlot(int index);

    // ── Settings helpers ──────────────────────────────────────────────────────
    void loadSlotIntoUI(int index);       // slot → sliders
    void saveUIIntoSlot(int index);       // sliders → slot
    void updatePreview();                 // redraws m_previewLabel
    void updateColorSquares();
    void updateDynamicSliders();
    void updateShapeButtons();
    void emitSettings();
    void saveLastColor();
    void loadSettings();

    // ── Data ──────────────────────────────────────────────────────────────────
    static constexpr int  kTotalSlots = 20;
    QVector<BrushSlot>    m_slots;
    int                   m_activeSlot  = 0;

    BrushEngine          *m_brushEngine = nullptr;   // non-owning, for preview
    bool                  m_built       = false;

    // ── Colour state ──────────────────────────────────────────────────────────
    QColor  m_primary   = Qt::black;
    QColor  m_secondary = Qt::white;

    // ── Colour section widgets ────────────────────────────────────────────────
    ColorWheelWidget   *m_colorWheel      = nullptr;
    QWidget            *m_swatchRow       = nullptr;
    QLabel             *m_primarySwatch   = nullptr;
    QLabel             *m_secondarySwatch = nullptr;

    // ── Brush body ────────────────────────────────────────────────────────────
    QWidget            *m_brushBody       = nullptr;

    // ── Slot grid ─────────────────────────────────────────────────────────────
    QWidget            *m_gridContainer   = nullptr;
    QGridLayout        *m_gridLayout      = nullptr;
    QVector<QToolButton*> m_gridBtns;

    // ── Preview + name ────────────────────────────────────────────────────────
    QLabel             *m_nameLbl         = nullptr;
    QLabel             *m_previewLabel    = nullptr;

    // ── Shape selector ────────────────────────────────────────────────────────
    QWidget            *m_shapeRow        = nullptr;
    QList<QToolButton*> m_shapeBtns;
    int                 m_activeShape     = 0;   // 0=Circle 1=Diamond 2=Square

    // ── Texture row ───────────────────────────────────────────────────────────
    QWidget            *m_textureRow      = nullptr;

    // ── Blend mode ────────────────────────────────────────────────────────────
    QComboBox          *m_blendCombo      = nullptr;
    QWidget            *m_blendRow        = nullptr;

    // ── Size & opacity sliders ────────────────────────────────────────────────
    QSlider            *m_sizeSlider         = nullptr;
    QSpinBox           *m_sizeSpin           = nullptr;
    QWidget            *m_minSizeRow         = nullptr;
    QSlider            *m_minSizeSlider      = nullptr;
    QSpinBox           *m_minSizeSpin        = nullptr;

    QSlider            *m_opacitySlider      = nullptr;
    QSpinBox           *m_opacitySpin        = nullptr;
    QWidget            *m_minOpacityRow      = nullptr;
    QSlider            *m_minOpacitySlider   = nullptr;
    QSpinBox           *m_minOpacitySpin     = nullptr;

    // ── Wet-media sliders ─────────────────────────────────────────────────────
    QWidget            *m_hardnessRow        = nullptr;
    QSlider            *m_hardnessSlider     = nullptr;
    QSpinBox           *m_hardnessSpin       = nullptr;

    QWidget            *m_blendingRow        = nullptr;
    QSlider            *m_blendingSlider     = nullptr;
    QSpinBox           *m_blendingSpin       = nullptr;

    QWidget            *m_dilutionRow        = nullptr;
    QSlider            *m_dilutionSlider     = nullptr;
    QSpinBox           *m_dilutionSpin       = nullptr;

    QWidget            *m_persistenceRow     = nullptr;
    QSlider            *m_persistenceSlider  = nullptr;
    QSpinBox           *m_persistenceSpin    = nullptr;

    QWidget            *m_blurPressureRow    = nullptr;
    QSlider            *m_blurPressureSlider = nullptr;
    QSpinBox           *m_blurPressureSpin   = nullptr;

    QWidget            *m_coloringRow        = nullptr;
    QSlider            *m_coloringSlider     = nullptr;
    QSpinBox           *m_coloringSpin       = nullptr;

    QWidget            *m_uncolorRow         = nullptr;
    QSlider            *m_uncolorSlider      = nullptr;
    QSpinBox           *m_uncolorSpin        = nullptr;

    QWidget            *m_blurWidthRow       = nullptr;
    QSlider            *m_blurWidthSlider    = nullptr;
    QSpinBox           *m_blurWidthSpin      = nullptr;

    // ── Tool state ────────────────────────────────────────────────────────────
    Tool          m_activeTool = Tool::Brush;
    BrushSettings m_settings;   // kept in sync with the active slot
};
