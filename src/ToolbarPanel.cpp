#include "ToolbarPanel.h"
#include "ColorWheelWidget.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QListWidget>
#include <QSpinBox>
#include <QSlider>
#include <QComboBox>
#include <QToolButton>
#include <QLabel>
#include <QColorDialog>
#include <QFrame>
#include <QButtonGroup>
#include <QPainter>
#include <QPixmap>
#include <cmath>

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────
static QFrame *makeSep(QWidget *parent)
{
    auto *f = new QFrame(parent);
    f->setFrameShape(QFrame::HLine);
    f->setFrameShadow(QFrame::Plain);
    f->setStyleSheet("background:#3a3a3a; max-height:1px; margin:2px 0;");
    return f;
}

static void addSliderRow(QWidget *parent, QVBoxLayout *layout,
                         const QString &label, int min, int max, int value,
                         QSlider **slOut, QSpinBox **spOut)
{
    auto *row = new QHBoxLayout;
    auto *lbl = new QLabel(label, parent);
    lbl->setFixedWidth(80);
    lbl->setStyleSheet("color:#ccc; font-size:11px;");
    auto *sl = new QSlider(Qt::Horizontal, parent);
    sl->setRange(min, max); sl->setValue(value);
    auto *sp = new QSpinBox(parent);
    sp->setRange(min, max); sp->setValue(value); sp->setFixedWidth(46);
    QObject::connect(sl, &QSlider::valueChanged, sp, &QSpinBox::setValue);
    QObject::connect(sp, QOverload<int>::of(&QSpinBox::valueChanged), sl, &QSlider::setValue);
    row->addWidget(lbl); row->addWidget(sl, 1); row->addWidget(sp);
    layout->addLayout(row);
    if (slOut) *slOut = sl;
    if (spOut) *spOut = sp;
}

// ─────────────────────────────────────────────────────────────────────────────
// Construction
// ─────────────────────────────────────────────────────────────────────────────
ToolbarPanel::ToolbarPanel(QObject *parent)
    : QObject(parent)
{
    // Nothing built yet — lazy init on first take*() call
}

void ToolbarPanel::ensureBuilt()
{
    if (m_built) return;
    m_built = true;
    buildColorSection();
    buildSwatchRow();
    buildBrushBody();
}

// ─────────────────────────────────────────────────────────────────────────────
// take*() methods — called by MainWindow to place sections into docks
// ─────────────────────────────────────────────────────────────────────────────
ColorWheelWidget *ToolbarPanel::takeColorWheel()
{
    ensureBuilt();
    return m_colorWheel;
}

QWidget *ToolbarPanel::takeSwatchRow(QWidget *newParent)
{
    ensureBuilt();
    if (m_swatchRow) m_swatchRow->setParent(newParent);
    return m_swatchRow;
}

QWidget *ToolbarPanel::takeBrushBody()
{
    ensureBuilt();
    return m_brushBody;
}

// ─────────────────────────────────────────────────────────────────────────────
// Section builders
// ─────────────────────────────────────────────────────────────────────────────
void ToolbarPanel::buildColorSection()
{
    m_colorWheel = new ColorWheelWidget(nullptr);
    m_colorWheel->setColor(m_primary);
    connect(m_colorWheel, &ColorWheelWidget::colorChanged, this, [this](const QColor &c) {
        m_primary = c;
        updateColorSquares();
        emit colorChanged(m_primary);
    });
}

void ToolbarPanel::buildSwatchRow()
{
    m_swatchRow = new QWidget(nullptr);
    auto *row = new QHBoxLayout(m_swatchRow);
    row->setContentsMargins(4, 2, 4, 2);
    row->setSpacing(6);

    auto *stack = new QWidget(m_swatchRow);
    stack->setFixedSize(56, 42);

    m_secondarySwatch = new QLabel(stack);
    m_secondarySwatch->setFixedSize(28, 28);
    m_secondarySwatch->setFrameShape(QFrame::Box);
    m_secondarySwatch->move(18, 12);

    m_primarySwatch = new QLabel(stack);
    m_primarySwatch->setFixedSize(32, 32);
    m_primarySwatch->setFrameShape(QFrame::Box);
    m_primarySwatch->move(0, 0);
    m_primarySwatch->setCursor(Qt::PointingHandCursor);

    auto *primaryBtn = new QToolButton(stack);
    primaryBtn->setGeometry(0, 0, 32, 32);
    primaryBtn->setStyleSheet("background:transparent; border:none;");
    connect(primaryBtn, &QToolButton::clicked, this, &ToolbarPanel::onPrimaryColorClicked);

    row->addWidget(stack);

    auto *swapBtn = new QToolButton(m_swatchRow);
    swapBtn->setText("⇄");
    swapBtn->setToolTip("Swap colors");
    swapBtn->setFixedSize(24, 24);
    connect(swapBtn, &QToolButton::clicked, this, &ToolbarPanel::onSwapColors);
    row->addWidget(swapBtn);
    row->addStretch();

    updateColorSquares();
}

void ToolbarPanel::buildBrushBody()
{
    m_brushBody = new QWidget(nullptr);
    auto *root = new QVBoxLayout(m_brushBody);
    root->setContentsMargins(6, 6, 6, 6);
    root->setSpacing(4);

    // ── Brush type tab rows ───────────────────────────────────────────────────
    struct BType { QString label; QString tip; BrushSettings::TipType tt; };
    static const BType types[] = {
        {"Pencil",   "Pencil",           BrushSettings::TipType::Pixel   },
        {"AirBrush", "Airbrush",         BrushSettings::TipType::Airbrush},
        {"Brush",    "Brush",            BrushSettings::TipType::Pixel   },
        {"Water",    "Watercolor",       BrushSettings::TipType::Pixel   },
        {"Marker",   "Marker",           BrushSettings::TipType::Pixel   },
        {"Eraser",   "Eraser (E)",       BrushSettings::TipType::Eraser  },
        {"SelPen",   "Selection Pen",    BrushSettings::TipType::Pixel   },
        {"SelErs",   "Selection Eraser", BrushSettings::TipType::Eraser  },
    };

    auto *grp  = new QButtonGroup(m_brushBody);
    grp->setExclusive(true);
    auto *row1 = new QHBoxLayout; row1->setSpacing(1);
    auto *row2 = new QHBoxLayout; row2->setSpacing(1);

    const QString tabStyle =
        "QToolButton { font-size:10px; padding:1px; border:1px solid #555;"
        "  background:#2d2d2d; color:#ccc; border-radius:2px; }"
        "QToolButton:checked { background:#4a6fa5; color:#fff; border-color:#5b83c0; }"
        "QToolButton:hover   { background:#383838; }";

    for (int i = 0; i < 8; ++i) {
        auto *btn = new QToolButton(m_brushBody);
        btn->setText(types[i].label);
        btn->setToolTip(types[i].tip);
        btn->setCheckable(true);
        btn->setFixedHeight(22);
        btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        btn->setStyleSheet(tabStyle);
        const int idx = i;
        connect(btn, &QToolButton::clicked, this, [this, idx]{ onBrushTypeSelected(idx); });
        grp->addButton(btn, i);
        m_brushTypeBtns.append(btn);
        (i < 4 ? row1 : row2)->addWidget(btn);
    }
    m_brushTypeBtns[0]->setChecked(true);
    root->addLayout(row1);
    root->addLayout(row2);
    root->addWidget(makeSep(m_brushBody));

    // ── Blend mode row ────────────────────────────────────────────────────────
    auto *blendRow = new QHBoxLayout;
    m_blendCombo = new QComboBox(m_brushBody);
    m_blendCombo->addItems({"Normal","Multiply","Screen","Overlay","Add","Luminosity"});
    m_blendCombo->setFixedWidth(96);
    connect(m_blendCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ToolbarPanel::onBlendModeChanged);
    blendRow->addWidget(m_blendCombo);

    for (const char *s : {"●","◐","◯","▬","✦"}) {
        auto *b = new QToolButton(m_brushBody);
        b->setText(QString::fromUtf8(s));
        b->setFixedSize(22, 22);
        b->setCheckable(true);
        b->setStyleSheet(
            "QToolButton { background:#2d2d2d; border:1px solid #555; color:#aaa;"
            " border-radius:2px; font-size:11px; }"
            "QToolButton:checked { background:#4a6fa5; border-color:#5b83c0; }");
        blendRow->addWidget(b);
    }
    blendRow->addStretch();
    root->addLayout(blendRow);
    root->addWidget(makeSep(m_brushBody));

    // ── Brush preset list ─────────────────────────────────────────────────────
    m_presetList = new QListWidget(m_brushBody);
    m_presetList->setFlow(QListView::TopToBottom);
    m_presetList->setWrapping(false);
    m_presetList->setViewMode(QListView::ListMode);
    m_presetList->setIconSize(QSize(40, 40));
    m_presetList->setSpacing(1);
    m_presetList->setStyleSheet(
        "QListWidget { background:#1a1a1a; border:1px solid #444; }"
        "QListWidget::item { padding:3px 4px; border-bottom:1px solid #2a2a2a; }"
        "QListWidget::item:selected { background:#4a6fa5; color:#fff; }"
        "QListWidget::item:hover { background:#2e2e2e; }");

    struct PInfo { QString name; BrushSettings::TipType tt; QColor c; float hard; };
    static const PInfo presets[] = {
        {"Round",       BrushSettings::TipType::Pixel,    {30,30,30},    0.90f},
        {"Soft Round",  BrushSettings::TipType::Pixel,    {30,30,30},    0.15f},
        {"Eraser",      BrushSettings::TipType::Eraser,   {210,210,210}, 0.85f},
        {"Airbrush",    BrushSettings::TipType::Airbrush, {60,80,170},   0.05f},
        {"Chalk",       BrushSettings::TipType::Chalk,    {90,90,90},    0.55f},
        {"Ink",         BrushSettings::TipType::Pixel,    {5,5,5},       1.00f},
        {"Smudge",      BrushSettings::TipType::Smudge,   {130,110,90},  0.65f},
        {"Texture",     BrushSettings::TipType::Pixel,    {60,60,60},    0.45f},
    };

    for (const auto &pr : presets) {
        QPixmap pix(40, 40);
        pix.fill(QColor("#1a1a1a"));
        QPainter pp(&pix);
        pp.setRenderHint(QPainter::Antialiasing);
        const int   N       = 20;
        const float isAir   = (pr.tt == BrushSettings::TipType::Airbrush);
        const float baseDiam = isAir ? 20.0f : 11.0f;
        const float baseAlpha = isAir ? 0.18f : (0.5f + pr.hard * 0.45f);
        const int   passes  = isAir ? 7 : (pr.hard < 0.4f ? 5 : 2);
        QColor c = pr.c;
        c.setAlphaF(std::clamp(static_cast<double>(baseAlpha / passes), 0.0, 1.0));
        pp.setPen(Qt::NoPen);
        pp.setBrush(c);
        for (int i = 0; i < N; ++i) {
            float t     = i / static_cast<float>(N - 1);
            float taper = 1.0f - std::abs(2.0f * t - 1.0f) * 0.5f;
            float x     = 3.0f + t * 34.0f;
            float r     = baseDiam * taper * 0.5f;
            for (int p2 = 0; p2 < passes; ++p2)
                pp.drawEllipse(QPointF(x, 20), r, r);
        }
        pp.end();

        auto *item = new QListWidgetItem(QIcon(pix), pr.name, m_presetList);
        item->setSizeHint(QSize(0, 48));
    }
    m_presetList->setCurrentRow(0);
    connect(m_presetList, &QListWidget::currentRowChanged,
            this, &ToolbarPanel::onBrushPresetSelected);
    root->addWidget(m_presetList);
    root->addWidget(makeSep(m_brushBody));

    // ── Sliders ───────────────────────────────────────────────────────────────
    addSliderRow(m_brushBody, root, tr("Brush Size"), 1, 500,
                 static_cast<int>(m_settings.size), &m_sizeSlider, &m_sizeSpin);
    connect(m_sizeSlider, &QSlider::valueChanged, this, &ToolbarPanel::onSizeChanged);

    QSlider *sl = nullptr; QSpinBox *sp = nullptr;
    addSliderRow(m_brushBody, root, tr("Min Size"),   0, 100, 0, &sl, &sp);
    Q_UNUSED(sl); Q_UNUSED(sp);

    addSliderRow(m_brushBody, root, tr("Density"),    0, 100,
                 static_cast<int>(m_settings.opacity * 100), &m_opacitySlider, &m_opacitySpin);
    connect(m_opacitySlider, &QSlider::valueChanged, this, &ToolbarPanel::onOpacityChanged);

    addSliderRow(m_brushBody, root, tr("Min Density"), 0, 100, 0, &sl, &sp);
    Q_UNUSED(sl); Q_UNUSED(sp);

    addSliderRow(m_brushBody, root, tr("Hardness"),   0, 100,
                 static_cast<int>(m_settings.hardness * 100), &m_hardnessSlider, &m_hardnessSpin);
    connect(m_hardnessSlider, &QSlider::valueChanged, this, &ToolbarPanel::onHardnessChanged);

    root->addStretch();
}

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────
void ToolbarPanel::updateColorSquares()
{
    auto css = [](const QColor &c) {
        return QString("background-color:%1; border:1px solid #888;").arg(c.name());
    };
    if (m_primarySwatch)   m_primarySwatch->setStyleSheet(css(m_primary));
    if (m_secondarySwatch) m_secondarySwatch->setStyleSheet(css(m_secondary));
}

void ToolbarPanel::emitSettings()
{
    emit brushSettingsChanged(m_settings);
}

// ─────────────────────────────────────────────────────────────────────────────
// Slots
// ─────────────────────────────────────────────────────────────────────────────
void ToolbarPanel::onPrimaryColorClicked()
{
    const QColor c = QColorDialog::getColor(m_primary, nullptr, tr("Primary Colour"));
    if (c.isValid()) {
        m_primary = c;
        if (m_colorWheel) m_colorWheel->setColor(c);
        updateColorSquares();
        emit colorChanged(m_primary);
    }
}

void ToolbarPanel::onSwapColors()
{
    std::swap(m_primary, m_secondary);
    if (m_colorWheel) m_colorWheel->setColor(m_primary);
    updateColorSquares();
    emit colorChanged(m_primary);
}

void ToolbarPanel::onSizeChanged(int v)
{
    m_settings.size = static_cast<float>(v);
    emitSettings();
}

void ToolbarPanel::onOpacityChanged(int v)
{
    m_settings.opacity = v / 100.0f;
    emitSettings();
}

void ToolbarPanel::onHardnessChanged(int v)
{
    m_settings.hardness = v / 100.0f;
    emitSettings();
}

void ToolbarPanel::onBlendModeChanged(int index)
{
    static const QPainter::CompositionMode modes[] = {
        QPainter::CompositionMode_SourceOver,
        QPainter::CompositionMode_Multiply,
        QPainter::CompositionMode_Screen,
        QPainter::CompositionMode_Overlay,
        QPainter::CompositionMode_Plus,
        QPainter::CompositionMode_HardLight,
    };
    if (index >= 0 && index < static_cast<int>(std::size(modes)))
        emit blendModeChanged(modes[index]);
    emitSettings();
}

void ToolbarPanel::onBrushTypeSelected(int idx)
{
    m_activeBrushType = idx;
    static const BrushSettings::TipType tipMap[] = {
        BrushSettings::TipType::Pixel,
        BrushSettings::TipType::Airbrush,
        BrushSettings::TipType::Pixel,
        BrushSettings::TipType::Pixel,
        BrushSettings::TipType::Pixel,
        BrushSettings::TipType::Eraser,
        BrushSettings::TipType::Pixel,
        BrushSettings::TipType::Eraser,
    };
    if (idx >= 0 && idx < 8) {
        m_settings.tipType = tipMap[idx];
        emit toolChanged(tipMap[idx] == BrushSettings::TipType::Eraser
                         ? Tool::Eraser : Tool::Brush);
    }
    emitSettings();
}

void ToolbarPanel::onBrushPresetSelected(int row)
{
    static const BrushSettings::TipType tipMap[] = {
        BrushSettings::TipType::Pixel,
        BrushSettings::TipType::Pixel,
        BrushSettings::TipType::Eraser,
        BrushSettings::TipType::Airbrush,
        BrushSettings::TipType::Chalk,
        BrushSettings::TipType::Pixel,
        BrushSettings::TipType::Smudge,
        BrushSettings::TipType::Pixel,
    };
    if (row >= 0 && row < static_cast<int>(std::size(tipMap)))
        m_settings.tipType = tipMap[row];

    if (row == 1) {   // Soft Round
        m_settings.hardness = 0.15f;
        if (m_hardnessSlider) { const QSignalBlocker b(m_hardnessSlider); m_hardnessSlider->setValue(15); }
        if (m_hardnessSpin)   { const QSignalBlocker b(m_hardnessSpin);   m_hardnessSpin->setValue(15);   }
    }
    emitSettings();
}

void ToolbarPanel::selectTool(Tool t)
{
    m_activeTool = t;
    if (t == Tool::Eraser) {
        m_settings.tipType = BrushSettings::TipType::Eraser;
        if (m_brushTypeBtns.size() > 5) m_brushTypeBtns[5]->setChecked(true);
    } else if (t == Tool::Brush) {
        m_settings.tipType = BrushSettings::TipType::Pixel;
        if (!m_brushTypeBtns.isEmpty()) m_brushTypeBtns[0]->setChecked(true);
    }
    emit toolChanged(t);
    emitSettings();
}
