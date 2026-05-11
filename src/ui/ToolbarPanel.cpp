#include "ToolbarPanel.h"
#include "ColorWheelWidget.h"
#include "BrushEngine.h"
#include "BrushTip.h"

#include <QSettings>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QScrollArea>
#include <QSpinBox>
#include <QSlider>
#include <QComboBox>
#include <QToolButton>
#include <QLabel>
#include <QColorDialog>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QFrame>
#include <QButtonGroup>
#include <QPainter>
#include <QPixmap>
#include <QMenu>
#include <QInputDialog>
#include <QStandardPaths>
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QFileDialog>
#include <cmath>

// ─────────────────────────────────────────────────────────────────────────────
// Static widget helpers
// ─────────────────────────────────────────────────────────────────────────────

static QFrame *makeSep(QWidget *parent)
{
    auto *f = new QFrame(parent);
    f->setFrameShape(QFrame::HLine);
    f->setFrameShadow(QFrame::Plain);
    f->setStyleSheet("background:#3a3a3a; max-height:1px; margin:2px 0;");
    return f;
}

static QWidget *makeSliderRow(QWidget *parent, QVBoxLayout *layout,
                               const QString &label, int min, int max, int value,
                               QSlider **slOut, QSpinBox **spOut)
{
    auto *container = new QWidget(parent);
    auto *row       = new QHBoxLayout(container);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(4);

    auto *lbl = new QLabel(label, container);
    lbl->setFixedWidth(80);
    lbl->setStyleSheet("color:#ccc; font-size:11px;");

    auto *sl = new QSlider(Qt::Horizontal, container);
    sl->setRange(min, max);
    sl->setValue(value);

    auto *sp = new QSpinBox(container);
    sp->setRange(min, max);
    sp->setValue(value);
    sp->setFixedWidth(46);

    QObject::connect(sl, &QSlider::valueChanged, sp, &QSpinBox::setValue);
    QObject::connect(sp, QOverload<int>::of(&QSpinBox::valueChanged), sl, &QSlider::setValue);

    row->addWidget(lbl);
    row->addWidget(sl, 1);
    row->addWidget(sp);
    layout->addWidget(container);

    if (slOut) *slOut = sl;
    if (spOut) *spOut = sp;
    return container;
}

// ─────────────────────────────────────────────────────────────────────────────
// Slot thumbnail pixmap
// ─────────────────────────────────────────────────────────────────────────────

static QPixmap makeSlotPixmap(const BrushSlot &slot, bool selected)
{
    const int W = 52, H = 52;
    QPixmap pix(W, H);
    pix.fill(selected ? QColor("#4a6fa5") : QColor("#2a2a2a"));

    if (slot.empty)
    {
        QPainter p(&pix);
        p.setPen(QPen(QColor("#666"), 1.5));
        p.drawLine(W / 2, H / 2 - 8, W / 2, H / 2 + 8);
        p.drawLine(W / 2 - 8, H / 2, W / 2 + 8, H / 2);
        return pix;
    }

    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);

    const TipType tt   = slot.settings.tipType;
    const float   hard = slot.settings.hardness;
    QColor        c(180, 180, 200);

    switch (tt)
    {
    case TipType::Airbrush:   c = QColor(100, 130, 220); break;
    case TipType::Brush:      c = QColor(100, 160, 210); break;
    case TipType::WaterColor: c = QColor(120, 180, 220); break;
    case TipType::Marker:     c = QColor( 80, 190, 140); break;
    case TipType::Eraser:     c = QColor(210, 210, 210); break;
    case TipType::Smudge:     c = QColor(190, 160, 120); break;
    case TipType::Blur:       c = QColor(160, 160, 200); break;
    case TipType::Bucket:     c = QColor(220, 190,  80); break;
    case TipType::Gradient:   c = QColor(180, 120, 220); break;
    case TipType::SelPen:     c = QColor(100, 220, 180); break;
    case TipType::SelEraser:  c = QColor(220, 100, 100); break;
    default:                  c = QColor(200, 200, 200); break;
    }

    const QPointF centre(W * 0.5f, H * 0.5f - 4);
    const float   r = 16.0f;

    if (tt == TipType::Airbrush || hard < 0.3f)
    {
        for (int ring = 4; ring >= 1; --ring)
        {
            c.setAlphaF(0.12f * ring);
            p.setBrush(c);
            p.drawEllipse(centre, ring * 4.5f, ring * 4.5f);
        }
    }
    else
    {
        c.setAlphaF(0.9f);
        p.setBrush(c);

        const int shape = slot.settings.brushShape;
        if (shape == 1)
        {
            // Diamond
            QPolygonF diamond;
            diamond << QPointF(centre.x(),     centre.y() - r)
                    << QPointF(centre.x() + r,  centre.y())
                    << QPointF(centre.x(),     centre.y() + r)
                    << QPointF(centre.x() - r,  centre.y());
            p.drawPolygon(diamond);
        }
        else if (shape == 2)
        {
            // Square
            p.drawRect(QRectF(centre.x() - r * 0.75f, centre.y() - r * 0.75f,
                              r * 1.5f, r * 1.5f));
        }
        else
        {
            // Circle (default)
            p.drawEllipse(centre, r, r);
        }
    }

    // Name label at bottom
    p.setPen(QColor("#eee"));
    QFont f; f.setPixelSize(9); p.setFont(f);
    p.drawText(QRectF(0, H - 16, W, 15),
               Qt::AlignHCenter | Qt::AlignVCenter,
               slot.name.left(8));

    return pix;
}

// ─────────────────────────────────────────────────────────────────────────────
// Default settings per tip type
// ─────────────────────────────────────────────────────────────────────────────

BrushSettings ToolbarPanel::defaultSettings(TipType tt)
{
    BrushSettings s;
    s.tipType = tt;
    switch (tt)
    {
    case TipType::Pixel:
        s.size = 10; s.hardness = 0.95f; s.spacing = 0.05f; s.sizeMultiplier = 1.0f; break;
    case TipType::Airbrush:
        s.size = 40; s.hardness = 0.0f;  s.spacing = 0.05f; s.sizeMultiplier = 1.0f; break;
    case TipType::Brush:
        s.size = 30; s.hardness = 0.80f; s.blending = 0.5f; s.persistence = 0.5f; break;
    case TipType::WaterColor:
        s.size = 30; s.hardness = 0.85f; s.blending = 0.3f; s.dilution = 0.3f;
        s.persistence = 0.5f; s.blurPressure = 0.5f; break;
    case TipType::Marker:
        s.size = 30; s.hardness = 0.95f; s.blending = 0.4f; s.persistence = 0.5f; break;
    case TipType::Eraser:
        s.size = 30; s.hardness = 0.85f; s.blendMode = BrushBlendMode::Erase; break;
    case TipType::Smudge:
        s.size = 30; s.hardness = 0.80f; s.coloring = 0.2f; s.uncolorPressure = 0.9f; break;
    case TipType::Blur:
        s.size = 30; s.blurWidth = 0.5f; break;
    default:
        s.size = 20; s.hardness = 0.85f; break;
    }
    return s;
}

// ─────────────────────────────────────────────────────────────────────────────
// Built-in slots (indices 0–9)
// ─────────────────────────────────────────────────────────────────────────────

static void fillBuiltinSlots(QVector<BrushSlot> &brush_slots)
{
    struct BuiltinDef { const char *name; TipType tt; };
    static const BuiltinDef builtins[10] = {
        { "Pencil",   TipType::Pixel      },
        { "AirBrush", TipType::Airbrush   },
        { "Brush",    TipType::Brush      },
        { "Water",    TipType::WaterColor  },
        { "Eraser",   TipType::Eraser     },
        { "Marker",   TipType::Marker     },
        { "Bucket",   TipType::Bucket     },
        { "Gradient", TipType::Gradient   },
        { "Blur",     TipType::Blur       },
        { "Smudge",   TipType::Smudge     },
    };
    for (int i = 0; i < 10; ++i)
    {
        BrushSlot sl;
        sl.empty    = false;
        sl.name     = builtins[i].name;
        sl.settings = ToolbarPanel::defaultSettings(builtins[i].tt);
        brush_slots[i] = sl;
    }
    // Slots 10–19 remain empty.
}

// ─────────────────────────────────────────────────────────────────────────────
// Construction
// ─────────────────────────────────────────────────────────────────────────────

ToolbarPanel::ToolbarPanel(BrushEngine *brushEngine, QObject *parent)
    : QObject(parent)
    , m_brushEngine(brushEngine)
{
    m_slots.resize(kTotalSlots);
    fillBuiltinSlots(m_slots);

    // Restore last used colour.
    QSettings settings("PixelCanvas", "PixelCanvas");
    const QString savedStr = settings.value("lastColor", "#ff000000").toString();
    const QColor  saved(savedStr);
    if (saved.isValid())
        m_primary = saved;
}

// ─────────────────────────────────────────────────────────────────────────────
// Lazy build  –  widgets are created on first use
// ─────────────────────────────────────────────────────────────────────────────

void ToolbarPanel::ensureBuilt()
{
    if (m_built) return;
    m_built = true;
    buildColorSection();
    buildSwatchRow();
    buildBrushBody();
    loadSettings();

    // Re-apply restored colour now that all widgets exist.
    if (m_colorWheel) m_colorWheel->setColor(m_primary);
    updateColorSquares();
    if (m_brushEngine) m_brushEngine->setColor(m_primary);
    emit colorChanged(m_primary);
}

// ─────────────────────────────────────────────────────────────────────────────
// take*()  –  hand widget ownership to the dock system
// ─────────────────────────────────────────────────────────────────────────────

ColorWheelWidget *ToolbarPanel::takeColorWheel()
{
    ensureBuilt();
    return m_colorWheel;
}

QWidget *ToolbarPanel::takeSwatchRow(QWidget *p)
{
    ensureBuilt();
    if (m_swatchRow) m_swatchRow->setParent(p);
    return m_swatchRow;
}

QWidget *ToolbarPanel::takeBrushBody()
{
    ensureBuilt();
    return m_brushBody;
}

// ─────────────────────────────────────────────────────────────────────────────
// Colour section
// ─────────────────────────────────────────────────────────────────────────────

void ToolbarPanel::buildColorSection()
{
    m_colorWheel = new ColorWheelWidget(nullptr);
    m_colorWheel->setColor(m_primary);
    connect(m_colorWheel, &ColorWheelWidget::colorChanged, this, [this](const QColor &c)
    {
        m_primary = c;
        updateColorSquares();
        emit colorChanged(m_primary);
        updatePreview();
        saveLastColor();
    });
}

void ToolbarPanel::buildSwatchRow()
{
    m_swatchRow = new QWidget(nullptr);
    auto *row   = new QHBoxLayout(m_swatchRow);
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
    swapBtn->setFixedSize(24, 24);
    connect(swapBtn, &QToolButton::clicked, this, &ToolbarPanel::onSwapColors);
    row->addWidget(swapBtn);
    row->addStretch();

    updateColorSquares();
}

// ─────────────────────────────────────────────────────────────────────────────
// Brush body
// ─────────────────────────────────────────────────────────────────────────────

void ToolbarPanel::buildBrushBody()
{
    m_brushBody = new QWidget(nullptr);
    auto *root  = new QVBoxLayout(m_brushBody);
    root->setContentsMargins(6, 6, 6, 6);
    root->setSpacing(4);

    // ── 1. Brush grid ─────────────────────────────────────────────────────────
    m_gridContainer = new QWidget(m_brushBody);
    m_gridLayout    = new QGridLayout(m_gridContainer);
    m_gridLayout->setSpacing(3);
    m_gridLayout->setContentsMargins(0, 0, 0, 0);

    m_gridBtns.resize(kTotalSlots, nullptr);
    rebuildGrid();

    auto *scrollArea = new QScrollArea(m_brushBody);
    scrollArea->setWidget(m_gridContainer);
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setFixedHeight(5 * 32 + 6);   // ~5 visible rows
    scrollArea->setStyleSheet("background:#1e1e1e;");
    root->addWidget(scrollArea);

    root->addWidget(makeSep(m_brushBody));

    // ── 2. Name + stroke preview ──────────────────────────────────────────────
    m_nameLbl = new QLabel(m_brushBody);
    m_nameLbl->setAlignment(Qt::AlignHCenter);
    m_nameLbl->setStyleSheet("color:#ccc; font-size:11px; font-weight:bold;");
    root->addWidget(m_nameLbl);

    m_previewLabel = new QLabel(m_brushBody);
    m_previewLabel->setFixedSize(250, 60);
    m_previewLabel->setAlignment(Qt::AlignCenter);
    m_previewLabel->setStyleSheet("background:#fff; border:1px solid #444;");
    root->addWidget(m_previewLabel);

    root->addWidget(makeSep(m_brushBody));

    // ── 3. Shape selector ─────────────────────────────────────────────────────
    m_shapeRow = new QWidget(m_brushBody);
    auto *shapeGroupLayout = new QVBoxLayout(m_shapeRow);
    shapeGroupLayout->setContentsMargins(0, 0, 0, 0);
    shapeGroupLayout->setSpacing(3);

    auto *shapeTopRow = new QWidget(m_shapeRow);
    auto *shapeLayout = new QHBoxLayout(shapeTopRow);
    shapeLayout->setContentsMargins(0, 0, 0, 0);
    shapeLayout->setSpacing(4);

    auto *shapeLbl = new QLabel(tr("Shape"), shapeTopRow);
    shapeLbl->setStyleSheet("color:#ccc; font-size:11px;");
    shapeLbl->setFixedWidth(80);
    shapeLayout->addWidget(shapeLbl);

    const QString shapeComboStyle =
        "QComboBox { background:#2d2d2d; border:1px solid #555; color:#ccc; "
        "border-radius:2px; padding:1px 4px; font-size:11px; }"
        "QComboBox::drop-down { border:none; }"
        "QComboBox QAbstractItemView { background:#2d2d2d; color:#ccc; "
        "selection-background-color:#4a6fa5; }";

    // Unified combo: indices 0–2 are built-ins, 3+ are imported BMPs.
    auto *shapeCombo = new QComboBox(shapeTopRow);
    shapeCombo->addItem(tr("Circle"),  QVariant());
    shapeCombo->addItem(tr("Diamond"), QVariant());
    shapeCombo->addItem(tr("Square"),  QVariant());
    shapeCombo->setStyleSheet(shapeComboStyle);

    const QString shapeDir = QStandardPaths::writableLocation(
        QStandardPaths::AppLocalDataLocation) + "/brush_shapes/";
    QDir(shapeDir).mkpath(".");
    for (const QString &f : QDir(shapeDir).entryList({"*.bmp", "*.png"}, QDir::Files))
        shapeCombo->addItem(QFileInfo(f).baseName(), shapeDir + f);

    shapeLayout->addWidget(shapeCombo, 1);

    // Import button
    auto *importShapeBtn = new QToolButton(shapeTopRow);
    importShapeBtn->setText("…");
    importShapeBtn->setToolTip(tr("Import brush shape BMP/PNG"));
    connect(importShapeBtn, &QToolButton::clicked, this, [this, shapeCombo]
    {
        const QString src = QFileDialog::getOpenFileName(
            nullptr, tr("Import Brush Shape"), {},
            tr("Images (*.bmp *.png)"));
        if (src.isEmpty()) return;

        const QString dest = QStandardPaths::writableLocation(
            QStandardPaths::AppLocalDataLocation)
            + "/brush_shapes/" + QFileInfo(src).fileName();

        for (int i = 3; i < shapeCombo->count(); ++i)
        {
            if (shapeCombo->itemData(i).toString() == dest)
            {
                shapeCombo->setCurrentIndex(i);
                return;
            }
        }
        if (QFile::exists(dest)) QFile::remove(dest);
        QFile::copy(src, dest);
        shapeCombo->addItem(QFileInfo(src).baseName(), dest);
        shapeCombo->setCurrentIndex(shapeCombo->count() - 1);
    });
    shapeLayout->addWidget(importShapeBtn);

    // Delete button — disabled for built-in entries (indices 0–2).
    auto *deleteShapeBtn = new QToolButton(shapeTopRow);
    deleteShapeBtn->setText("✕");
    deleteShapeBtn->setToolTip(tr("Delete selected shape"));
    deleteShapeBtn->setFixedWidth(22);
    deleteShapeBtn->setEnabled(false);
    deleteShapeBtn->setStyleSheet(
        "QToolButton { background:#5a2020; border:1px solid #844; color:#fcc; "
        "border-radius:2px; font-size:10px; }"
        "QToolButton:hover    { background:#7a2020; }"
        "QToolButton:disabled { background:#333; color:#666; border-color:#444; }");
    connect(deleteShapeBtn, &QToolButton::clicked, this, [this, shapeCombo]
    {
        const int idx = shapeCombo->currentIndex();
        if (idx < 3) return;
        const QString path = shapeCombo->itemData(idx).toString();
        if (!path.isEmpty()) QFile::remove(path);
        shapeCombo->removeItem(idx);
        shapeCombo->setCurrentIndex(0);
        if (m_brushEngine) m_brushEngine->setTipShape({});
        m_settings.brushShape = 0;
        saveUIIntoSlot(m_activeSlot);
        emitSettings();
    });
    shapeLayout->addWidget(deleteShapeBtn);

    connect(shapeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this, shapeCombo, deleteShapeBtn](int idx)
    {
        deleteShapeBtn->setEnabled(idx >= 3);
        if (idx < 3)
        {
            m_settings.brushShape = idx;
            if (m_brushEngine) m_brushEngine->setTipShape({});
        }
        else
        {
            m_settings.brushShape = 0;
            const QString path = shapeCombo->itemData(idx).toString();
            if (m_brushEngine)
            {
                m_brushEngine->setTipShape(path);
                m_settings.spacing = m_brushEngine->settings().spacing;
            }
        }
        m_activeShape = idx;
        saveUIIntoSlot(m_activeSlot);
        updatePreview();
        emitSettings();
    });

    shapeGroupLayout->addWidget(shapeTopRow);
    root->addWidget(m_shapeRow);

    m_shapeRow->setProperty("shapeCombo",
        QVariant::fromValue(static_cast<QObject *>(shapeCombo)));
    m_shapeRow->setProperty("shapeBmpCombo",
        QVariant::fromValue(static_cast<QObject *>(shapeCombo)));

    // ── 4. Texture import row ─────────────────────────────────────────────────
    m_textureRow = new QWidget(m_brushBody);
    auto *texGroupLayout = new QVBoxLayout(m_textureRow);
    texGroupLayout->setContentsMargins(0, 0, 0, 0);
    texGroupLayout->setSpacing(3);

    // Row A: combo + import + delete
    auto *texTopRow = new QWidget(m_textureRow);
    auto *texLayout = new QHBoxLayout(texTopRow);
    texLayout->setContentsMargins(0, 0, 0, 0);
    texLayout->setSpacing(4);

    auto *texLbl = new QLabel(tr("Texture"), texTopRow);
    texLbl->setStyleSheet("color:#ccc; font-size:11px;");
    texLbl->setFixedWidth(80);
    texLayout->addWidget(texLbl);

    const QString comboStyle =
        "QComboBox { background:#2d2d2d; border:1px solid #555; color:#ccc; "
        "border-radius:2px; padding:1px 4px; font-size:11px; }"
        "QComboBox::drop-down { border:none; }"
        "QComboBox QAbstractItemView { background:#2d2d2d; color:#ccc; "
        "selection-background-color:#4a6fa5; }";

    auto *texCombo = new QComboBox(texTopRow);
    texCombo->addItem(tr("None"));
    texCombo->setStyleSheet(comboStyle);

    const QString texDir = QStandardPaths::writableLocation(
        QStandardPaths::AppLocalDataLocation) + "/brush_textures/";
    QDir(texDir).mkpath(".");
    for (const QString &f : QDir(texDir).entryList({"*.bmp", "*.png"}, QDir::Files))
        texCombo->addItem(QFileInfo(f).baseName(), texDir + f);

    texLayout->addWidget(texCombo, 1);

    // Import button
    auto *importTexBtn = new QToolButton(texTopRow);
    importTexBtn->setText("…");
    importTexBtn->setToolTip(tr("Import texture BMP/PNG"));
    connect(importTexBtn, &QToolButton::clicked, this, [this, texCombo]
    {
        const QString src = QFileDialog::getOpenFileName(
            nullptr, tr("Import Brush Texture"), {},
            tr("Images (*.bmp *.png *.jpg)"));
        if (src.isEmpty()) return;

        const QString dest = QStandardPaths::writableLocation(
            QStandardPaths::AppLocalDataLocation)
            + "/brush_textures/" + QFileInfo(src).fileName();

        for (int i = 1; i < texCombo->count(); ++i)
        {
            if (texCombo->itemData(i).toString() == dest)
            {
                if (texCombo->currentIndex() == i)
                {
                    if (m_brushEngine) m_brushEngine->setTipTexture(dest);
                    updatePreview();
                }
                else
                {
                    texCombo->setCurrentIndex(i);
                }
                return;
            }
        }
        if (QFile::exists(dest)) QFile::remove(dest);
        QFile::copy(src, dest);
        texCombo->addItem(QFileInfo(src).baseName(), dest);
        texCombo->setCurrentIndex(texCombo->count() - 1);
    });
    texLayout->addWidget(importTexBtn);

    // Delete button — disabled when "None" is selected.
    auto *deleteTexBtn = new QToolButton(texTopRow);
    deleteTexBtn->setText("✕");
    deleteTexBtn->setToolTip(tr("Delete selected texture"));
    deleteTexBtn->setFixedWidth(22);
    deleteTexBtn->setEnabled(false);
    deleteTexBtn->setStyleSheet(
        "QToolButton { background:#5a2020; border:1px solid #844; color:#fcc; "
        "border-radius:2px; font-size:10px; }"
        "QToolButton:hover    { background:#7a2020; }"
        "QToolButton:disabled { background:#333; color:#666; border-color:#444; }");
    connect(deleteTexBtn, &QToolButton::clicked, this, [this, texCombo]
    {
        const int idx = texCombo->currentIndex();
        if (idx <= 0) return;
        const QString path = texCombo->itemData(idx).toString();
        if (!path.isEmpty()) QFile::remove(path);
        texCombo->removeItem(idx);
        texCombo->setCurrentIndex(0);
        if (m_brushEngine) m_brushEngine->setTipTexture({});
        updatePreview();
    });
    texLayout->addWidget(deleteTexBtn);
    texGroupLayout->addWidget(texTopRow);

    // Row B: texture strength slider
    auto *texStrRow    = new QWidget(m_textureRow);
    auto *texStrLayout = new QHBoxLayout(texStrRow);
    texStrLayout->setContentsMargins(0, 0, 0, 0);
    texStrLayout->setSpacing(4);

    auto *texStrLbl = new QLabel(tr("Strength"), texStrRow);
    texStrLbl->setStyleSheet("color:#ccc; font-size:11px;");
    texStrLbl->setFixedWidth(80);
    texStrLayout->addWidget(texStrLbl);

    auto *texStrSlider = new QSlider(Qt::Horizontal, texStrRow);
    texStrSlider->setRange(0, 100);
    texStrSlider->setValue(100);

    auto *texStrSpin = new QSpinBox(texStrRow);
    texStrSpin->setRange(0, 100);
    texStrSpin->setValue(100);
    texStrSpin->setFixedWidth(46);

    QObject::connect(texStrSlider, &QSlider::valueChanged,
                     texStrSpin,   &QSpinBox::setValue);
    QObject::connect(texStrSpin, QOverload<int>::of(&QSpinBox::valueChanged),
                     texStrSlider, &QSlider::setValue);
    QObject::connect(texStrSlider, &QSlider::valueChanged, this, [this](int v)
    {
        m_settings.textureStrength = v / 100.0f;
        saveUIIntoSlot(m_activeSlot);
        updatePreview();
        emitSettings();
    });

    texStrLayout->addWidget(texStrSlider, 1);
    texStrLayout->addWidget(texStrSpin);
    texGroupLayout->addWidget(texStrRow);
    texStrRow->setVisible(false);   // hidden until a texture is selected

    connect(texCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this, texCombo, deleteTexBtn, texStrRow](int idx)
    {
        deleteTexBtn->setEnabled(idx > 0);
        texStrRow->setVisible(idx > 0);
        const QString path = texCombo->itemData(idx).toString();
        if (m_brushEngine) m_brushEngine->setTipTexture(path);
        updatePreview();
    });

    m_textureRow->setProperty("texCombo",
        QVariant::fromValue(static_cast<QObject *>(texCombo)));

    root->addWidget(m_textureRow);
    root->addWidget(makeSep(m_brushBody));

    // ── 5. Sliders ────────────────────────────────────────────────────────────
    makeSliderRow(m_brushBody, root, tr("Brush Size"), 1, 500,
                  (int)m_settings.size, &m_sizeSlider, &m_sizeSpin);
    connect(m_sizeSlider, &QSlider::valueChanged, this, &ToolbarPanel::onSizeChanged);

    m_minSizeRow = makeSliderRow(m_brushBody, root, tr("Min Size"), 0, 100,
                  (int)(m_settings.minSizeFraction * 100), &m_minSizeSlider, &m_minSizeSpin);
    connect(m_minSizeSlider, &QSlider::valueChanged, this, &ToolbarPanel::onMinSizeChanged);

    makeSliderRow(m_brushBody, root, tr("Density"), 0, 100,
                  (int)(m_settings.opacity * 100), &m_opacitySlider, &m_opacitySpin);
    connect(m_opacitySlider, &QSlider::valueChanged, this, &ToolbarPanel::onOpacityChanged);

    m_minOpacityRow = makeSliderRow(m_brushBody, root, tr("Min Density"), 0, 100,
                  (int)(m_settings.minOpacity * 100), &m_minOpacitySlider, &m_minOpacitySpin);
    connect(m_minOpacitySlider, &QSlider::valueChanged, this, &ToolbarPanel::onMinOpacityChanged);

    m_hardnessRow = makeSliderRow(m_brushBody, root, tr("Hardness"), 0, 100,
                  (int)(m_settings.hardness * 100), &m_hardnessSlider, &m_hardnessSpin);
    connect(m_hardnessSlider, &QSlider::valueChanged, this, &ToolbarPanel::onHardnessChanged);

    m_blendingRow = makeSliderRow(m_brushBody, root, tr("Blending"), 0, 100,
                  (int)(m_settings.blending * 100), &m_blendingSlider, &m_blendingSpin);
    connect(m_blendingSlider, &QSlider::valueChanged, this, &ToolbarPanel::onBlendingChanged);

    m_dilutionRow = makeSliderRow(m_brushBody, root, tr("Dilution"), 0, 100,
                  (int)(m_settings.dilution * 100), &m_dilutionSlider, &m_dilutionSpin);
    connect(m_dilutionSlider, &QSlider::valueChanged, this, &ToolbarPanel::onDilutionChanged);

    m_persistenceRow = makeSliderRow(m_brushBody, root, tr("Persistence"), 0, 100,
                  (int)(m_settings.persistence * 100), &m_persistenceSlider, &m_persistenceSpin);
    connect(m_persistenceSlider, &QSlider::valueChanged, this, &ToolbarPanel::onPersistenceChanged);

    m_blurPressureRow = makeSliderRow(m_brushBody, root, tr("Blur Pressure"), 0, 100,
                  (int)(m_settings.blurPressure * 100), &m_blurPressureSlider, &m_blurPressureSpin);
    connect(m_blurPressureSlider, &QSlider::valueChanged, this, &ToolbarPanel::onBlurPressureChanged);

    m_coloringRow = makeSliderRow(m_brushBody, root, tr("Coloring"), 0, 100,
                  (int)(m_settings.coloring * 100), &m_coloringSlider, &m_coloringSpin);
    connect(m_coloringSlider, &QSlider::valueChanged, this, &ToolbarPanel::onColoringChanged);

    m_uncolorRow = makeSliderRow(m_brushBody, root, tr("Uncolor Prs."), 0, 100,
                  (int)(m_settings.uncolorPressure * 100), &m_uncolorSlider, &m_uncolorSpin);
    connect(m_uncolorSlider, &QSlider::valueChanged, this, &ToolbarPanel::onUncolorPressureChanged);

    m_blurWidthRow = makeSliderRow(m_brushBody, root, tr("Blur Width"), 0, 100,
                  (int)(m_settings.blurWidth * 100), &m_blurWidthSlider, &m_blurWidthSpin);
    connect(m_blurWidthSlider, &QSlider::valueChanged, this, &ToolbarPanel::onBlurWidthChanged);

    // ── 6. Blend mode ─────────────────────────────────────────────────────────
    root->addWidget(makeSep(m_brushBody));

    auto *blendContainer = new QWidget(m_brushBody);
    auto *blendRowLayout = new QHBoxLayout(blendContainer);
    blendRowLayout->setContentsMargins(0, 0, 0, 0);
    blendRowLayout->setSpacing(4);

    auto *blendLbl = new QLabel(tr("Blend Mode"), blendContainer);
    blendLbl->setFixedWidth(80);
    blendLbl->setStyleSheet("color:#ccc; font-size:11px;");

    m_blendCombo = new QComboBox(blendContainer);
    m_blendCombo->addItems({ "Normal", "Multiply", "Screen", "Overlay",
                              "Luminosity", "Shade", "Vivid", "Deep" });
    connect(m_blendCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ToolbarPanel::onBlendModeChanged);

    blendRowLayout->addWidget(blendLbl);
    blendRowLayout->addWidget(m_blendCombo, 1);
    m_blendRow = blendContainer;
    root->addWidget(blendContainer);

    root->addStretch();

    selectSlot(0);
}

// ─────────────────────────────────────────────────────────────────────────────
// Grid
// ─────────────────────────────────────────────────────────────────────────────

void ToolbarPanel::rebuildGrid()
{
    for (auto *btn : m_gridBtns)
        if (btn) { m_gridLayout->removeWidget(btn); delete btn; }
    m_gridBtns.fill(nullptr);

    const QString baseStyle =
        "QToolButton { background:#2a2a2a; border:1px solid #444; border-radius:3px; "
        "color:#ccc; font-size:10px; padding:2px; }"
        "QToolButton:hover { background:#383838; border-color:#666; }";
    const QString selStyle =
        "QToolButton { background:#4a6fa5; border:2px solid #7aaff0; border-radius:3px; "
        "color:#fff; font-size:10px; font-weight:bold; padding:2px; }";

    for (int i = 0; i < kTotalSlots; ++i)
    {
        auto *btn = new QToolButton(m_gridContainer);
        btn->setFixedSize(54, 28);
        btn->setToolButtonStyle(Qt::ToolButtonTextOnly);
        btn->setText(m_slots[i].empty ? "+" : m_slots[i].name);
        btn->setStyleSheet(i == m_activeSlot ? selStyle : baseStyle);

        const int idx = i;
        connect(btn, &QToolButton::clicked, this, [this, idx]
        {
            if (m_slots[idx].empty)
                addCustomBrush(idx);
            else
                selectSlot(idx);
        });

        // Right-click context menu for custom (non-builtin) slots.
        if (!m_slots[i].empty && i >= 10)
        {
            btn->setContextMenuPolicy(Qt::CustomContextMenu);
            connect(btn, &QToolButton::customContextMenuRequested, this,
                    [this, idx](const QPoint &pt)
            {
                QMenu menu;
                QAction *del = menu.addAction(tr("Remove"));
                if (menu.exec(m_gridBtns[idx]->mapToGlobal(pt)) == del)
                    removeSlot(idx);
            });
        }

        m_gridBtns[i] = btn;
        m_gridLayout->addWidget(btn, i / 4, i % 4);
    }
}

void ToolbarPanel::selectSlot(int index)
{
    if (index < 0 || index >= kTotalSlots) return;
    if (m_slots[index].empty) return;

    // Save current UI settings back to the previous slot.
    if (!m_slots[m_activeSlot].empty)
        saveUIIntoSlot(m_activeSlot);

    const QString baseStyle =
        "QToolButton { background:#2a2a2a; border:1px solid #444; border-radius:3px; "
        "color:#ccc; font-size:10px; padding:2px; }"
        "QToolButton:hover { background:#383838; border-color:#666; }";
    const QString selStyle =
        "QToolButton { background:#4a6fa5; border:2px solid #7aaff0; border-radius:3px; "
        "color:#fff; font-size:10px; font-weight:bold; padding:2px; }";

    if (m_gridBtns[m_activeSlot]) m_gridBtns[m_activeSlot]->setStyleSheet(baseStyle);
    m_activeSlot = index;
    if (m_gridBtns[m_activeSlot]) m_gridBtns[m_activeSlot]->setStyleSheet(selStyle);

    loadSlotIntoUI(index);
}

void ToolbarPanel::addCustomBrush(int slotIndex)
{
    if (slotIndex < 10 || slotIndex >= kTotalSlots) return;

    QDialog dlg;
    dlg.setWindowTitle(tr("New Brush"));
    auto *dlgLayout = new QVBoxLayout(&dlg);

    auto *nameEdit = new QLineEdit(&dlg);
    nameEdit->setMaxLength(10);
    nameEdit->setPlaceholderText(tr("Name (max 10 chars)"));
    dlgLayout->addWidget(new QLabel(tr("Brush name:"), &dlg));
    dlgLayout->addWidget(nameEdit);

    auto *typeCombo = new QComboBox(&dlg);
    typeCombo->addItems({ "Pencil", "AirBrush", "Brush", "WaterColor",
                          "Eraser", "Marker", "Bucket", "Gradient", "Blur", "Smudge" });
    dlgLayout->addWidget(new QLabel(tr("Base type:"), &dlg));
    dlgLayout->addWidget(typeCombo);

    auto *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    dlgLayout->addWidget(bb);

    if (dlg.exec() != QDialog::Accepted) return;

    const QString name = nameEdit->text().trimmed();
    if (name.isEmpty()) return;

    static const TipType typeMap[] = {
        TipType::Pixel, TipType::Airbrush, TipType::Brush,    TipType::WaterColor,
        TipType::Eraser, TipType::Marker,  TipType::SelPen,   TipType::SelEraser,
        TipType::Bucket, TipType::Gradient, TipType::Blur,    TipType::Smudge,
    };
    const TipType tt = typeMap[typeCombo->currentIndex()];

    BrushSlot &sl = m_slots[slotIndex];
    sl.empty    = false;
    sl.name     = name;
    sl.settings = defaultSettings(tt);

    rebuildGrid();
    selectSlot(slotIndex);
}

void ToolbarPanel::removeSlot(int index)
{
    if (index < 10 || index >= kTotalSlots) return;

    m_slots[index] = BrushSlot{};

    if (m_activeSlot == index)
    {
        m_activeSlot = 0;
        loadSlotIntoUI(0);
    }
    rebuildGrid();

    if (m_gridBtns[m_activeSlot])
    {
        m_gridBtns[m_activeSlot]->setStyleSheet(
            "QToolButton { background:#4a6fa5; border:2px solid #7aaff0; border-radius:3px; "
            "color:#fff; font-size:10px; font-weight:bold; padding:2px; }");
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Load / save slot ↔ UI
// ─────────────────────────────────────────────────────────────────────────────

void ToolbarPanel::loadSlotIntoUI(int index)
{
    if (index < 0 || index >= kTotalSlots || m_slots[index].empty) return;
    const BrushSettings &s = m_slots[index].settings;
    m_settings = s;

    // Block all signals while loading to avoid re-entering emitSettings().
    auto block = [](QSlider *sl, QSpinBox *sp, int v)
    {
        if (sl) { const QSignalBlocker b(sl); sl->setValue(v); }
        if (sp) { const QSignalBlocker b(sp); sp->setValue(v); }
    };

    block(m_sizeSlider,         m_sizeSpin,         (int)s.size);
    block(m_minSizeSlider,      m_minSizeSpin,      (int)(s.minSizeFraction   * 100));
    block(m_opacitySlider,      m_opacitySpin,      (int)(s.opacity           * 100));
    block(m_minOpacitySlider,   m_minOpacitySpin,   (int)(s.minOpacity        * 100));
    block(m_hardnessSlider,     m_hardnessSpin,     (int)(s.hardness          * 100));
    block(m_blendingSlider,     m_blendingSpin,     (int)(s.blending          * 100));
    block(m_dilutionSlider,     m_dilutionSpin,     (int)(s.dilution          * 100));
    block(m_persistenceSlider,  m_persistenceSpin,  (int)(s.persistence       * 100));
    block(m_blurPressureSlider, m_blurPressureSpin, (int)(s.blurPressure      * 100));
    block(m_coloringSlider,     m_coloringSpin,     (int)(s.coloring          * 100));
    block(m_uncolorSlider,      m_uncolorSpin,      (int)(s.uncolorPressure   * 100));
    block(m_blurWidthSlider,    m_blurWidthSpin,    (int)(s.blurWidth         * 100));

    // Sync blend mode combo.
    static const BrushBlendMode modes[] = {
        BrushBlendMode::Normal,    BrushBlendMode::Multiply,
        BrushBlendMode::Screen,    BrushBlendMode::Overlay,
        BrushBlendMode::Luminosity, BrushBlendMode::Shade,
        BrushBlendMode::Vivid,     BrushBlendMode::Deep,
    };
    if (m_blendCombo)
    {
        const QSignalBlocker b(m_blendCombo);
        for (int i = 0; i < (int)std::size(modes); ++i)
        {
            if (modes[i] == s.blendMode) { m_blendCombo->setCurrentIndex(i); break; }
        }
    }

    // Restore shape / texture into engine.
    // Built-in shapes 1 (diamond) and 2 (square) must clear the BMP path so
    // PixelTip is used and brushShape is respected.  Only index 0 (circle)
    // can have an imported BMP path.
    if (m_brushEngine)
    {
        if (s.brushShape == 1 || s.brushShape == 2)
        {
            m_brushEngine->setTipShape({});
            m_brushEngine->setTipTexture(m_slots[index].texturePath);
        }
        else
        {
            m_brushEngine->setTipShape(m_slots[index].shapePath);
            m_brushEngine->setTipTexture(m_slots[index].texturePath);
        }
    }

    // Restore shape combo selection.
    if (m_shapeRow)
    {
        auto *shapeCombo = qobject_cast<QComboBox *>(
            qvariant_cast<QObject *>(m_shapeRow->property("shapeCombo")));
        if (shapeCombo)
        {
            const QSignalBlocker b(shapeCombo);
            if (s.brushShape == 1 || s.brushShape == 2)
            {
                shapeCombo->setCurrentIndex(s.brushShape);
            }
            else if (!m_slots[index].shapePath.isEmpty())
            {
                bool found = false;
                for (int i = 3; i < shapeCombo->count(); ++i)
                {
                    if (shapeCombo->itemData(i).toString() == m_slots[index].shapePath)
                    {
                        shapeCombo->setCurrentIndex(i);
                        found = true;
                        break;
                    }
                }
                if (!found) shapeCombo->setCurrentIndex(0);
            }
            else
            {
                shapeCombo->setCurrentIndex(0);
            }
        }
    }

    // Restore texture combo selection.
    if (m_textureRow)
    {
        auto *texCombo = qobject_cast<QComboBox *>(
            qvariant_cast<QObject *>(m_textureRow->property("texCombo")));
        if (texCombo)
        {
            const QSignalBlocker b(texCombo);
            if (!m_slots[index].texturePath.isEmpty())
            {
                bool found = false;
                for (int i = 1; i < texCombo->count(); ++i)
                {
                    if (texCombo->itemData(i).toString() == m_slots[index].texturePath)
                    {
                        texCombo->setCurrentIndex(i);
                        found = true;
                        break;
                    }
                }
                if (!found) texCombo->setCurrentIndex(0);
            }
            else
            {
                texCombo->setCurrentIndex(0);
            }
        }
    }

    m_activeShape = s.brushShape;

    if (m_nameLbl) m_nameLbl->setText(m_slots[index].name);

    updateDynamicSliders();
    updateShapeButtons();
    updatePreview();
    emitSettings();
}

void ToolbarPanel::saveUIIntoSlot(int index)
{
    if (index < 0 || index >= kTotalSlots || m_slots[index].empty) return;
    m_slots[index].settings = m_settings;
    if (m_brushEngine)
    {
        // Built-in shapes 1/2 (diamond/square) never use a BMP path.
        m_slots[index].shapePath = (m_settings.brushShape == 1 || m_settings.brushShape == 2)
                                   ? QString{}
                                   : m_brushEngine->currentShapePath();
        m_slots[index].texturePath = m_brushEngine->currentTexturePath();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Stroke preview
// ─────────────────────────────────────────────────────────────────────────────

void ToolbarPanel::updatePreview()
{
    if (!m_previewLabel) return;

    const BrushSettings &s  = m_settings;
    const TipType        tt = s.tipType;

    const bool canDraw = (tt != TipType::Bucket  && tt != TipType::Gradient &&
                          tt != TipType::Blur     && tt != TipType::Smudge   &&
                          tt != TipType::SelPen   && tt != TipType::SelEraser);

    m_previewLabel->setVisible(canDraw);
    if (!canDraw || m_slots[m_activeSlot].empty) return;

    const int W = m_previewLabel->width()  > 0 ? m_previewLabel->width()  : 200;
    const int H = m_previewLabel->height() > 0 ? m_previewLabel->height() : 60;

    QImage img(W, H, QImage::Format_ARGB32_Premultiplied);

    const bool isWetTip = (tt == TipType::Brush ||
                           tt == TipType::WaterColor ||
                           tt == TipType::Marker);

    if (tt == TipType::Eraser) img.fill(Qt::black);
    else if (isWetTip)          img.fill(QColor("#666"));
    else                        img.fill(QColor("#111"));

    // Cap preview size to fit the preview strip.
    BrushSettings ps  = s;
    ps.size           = std::min(s.effectiveDiameter(), (float)(H - 8));
    ps.sizeMultiplier = 1.0f;

    const float cy      = H * 0.5f;
    const float r       = ps.size * 0.5f;
    const float spacing = std::max(r * s.spacing, 1.0f);

    // Build the correct tip, applying shape/texture BMPs where available.
    BrushTip *tip = nullptr;
    switch (tt)
    {
    case TipType::Airbrush:
        tip = new AirbrushTip;
        break;

    case TipType::Brush:
    case TipType::WaterColor:
    case TipType::Marker:
        if (m_brushEngine)
        {
            const QString texPath   = m_brushEngine->currentTexturePath();
            const QString shapePath = m_brushEngine->currentShapePath();
            if (!texPath.isEmpty())
            {
                tip = new TextureTip(texPath, shapePath);
                break;
            }
        }
        if      (tt == TipType::Brush)      tip = new BrushTipWet;
        else if (tt == TipType::WaterColor)  tip = new WaterColorTip;
        else                                 tip = new MarkerTip;
        break;

    case TipType::Chalk:
        tip = new ChalkTip;
        break;

    case TipType::Eraser:
        if (m_brushEngine)
        {
            const QString shapePath = m_brushEngine->currentShapePath();
            const QString texPath   = m_brushEngine->currentTexturePath();
            if (!shapePath.isEmpty() || !texPath.isEmpty())
            {
                tip = new TextureTip(texPath, shapePath);
                break;
            }
        }
        tip = new EraserTip;
        break;

    default:
        if (m_brushEngine)
        {
            const QString shapePath = m_brushEngine->currentShapePath();
            const QString texPath   = m_brushEngine->currentTexturePath();
            if (!shapePath.isEmpty() || !texPath.isEmpty())
            {
                tip = new TextureTip(texPath, shapePath);
                break;
            }
        }
        tip = new PixelTip;
        break;
    }

    // For wet tips, load the shape BMP mask if one is active.
    QImage previewShapeMask;
    if (isWetTip && m_brushEngine && !m_brushEngine->currentShapePath().isEmpty())
        previewShapeMask = TextureTip::loadGrayscaleMaskPublic(
            m_brushEngine->currentShapePath());

    if (tip) tip->beginStroke();

    DabParams dab;
    dab.diameter        = ps.size;
    dab.hardness        = s.hardness;
    dab.opacity         = s.opacity;
    dab.pressure        = 1.0f;
    dab.color           = m_primary;
    dab.blending        = 0.0f;   // suppressed in preview
    dab.dilution        = 0.0f;   // suppressed in preview
    dab.persistence     = s.persistence;
    dab.blurPressure    = s.blurPressure;
    dab.coloring        = s.coloring;
    dab.uncolorPressure = s.uncolorPressure;
    dab.blurWidth       = s.blurWidth;
    dab.keepOpacity     = s.keepOpacity;
    dab.brushShape      = s.brushShape;
    dab.textureStrength = s.textureStrength;
    dab.shapeMask       = previewShapeMask.isNull() ? nullptr : &previewShapeMask;

    const QPainter::CompositionMode compMode = (tt == TipType::Eraser)
        ? QPainter::CompositionMode_DestinationOut
        : QPainter::CompositionMode_SourceOver;

    float x = r;
    while (x < W - r)
    {
        const float wave = std::sin(x * 0.05f) * (H * 0.18f);
        dab.center = QPointF(x, cy + wave);

        // Open and close the painter each dab so wet tips can safely call
        // constScanLine() on img between dabs (active QPainter invalidates reads).
        QPainter p(&img);
        p.setCompositionMode(compMode);
        if (tip) tip->stamp(p, dab);
        p.end();

        x += spacing;
    }
    delete tip;

    m_previewLabel->setPixmap(QPixmap::fromImage(img));
}

// ─────────────────────────────────────────────────────────────────────────────
// Dynamic slider / row visibility
// ─────────────────────────────────────────────────────────────────────────────

void ToolbarPanel::updateDynamicSliders()
{
    const TipType tt = m_settings.tipType;

    const bool isWet       = (tt == TipType::Brush || tt == TipType::WaterColor || tt == TipType::Marker);
    const bool hasDilution = (tt == TipType::Brush || tt == TipType::WaterColor);
    const bool hasBlurPrs  = (tt == TipType::WaterColor || tt == TipType::Marker);
    const bool isSmudge    = (tt == TipType::Smudge);
    const bool isBlur      = (tt == TipType::Blur);
    const bool isBucket    = (tt == TipType::Bucket || tt == TipType::Gradient);
    const bool hasHard     = !isBucket && !isBlur;
    const bool hasMinCtrls = !isBucket && !isBlur;
    const bool showBase    = !isBucket;

    if (m_sizeSlider)     m_sizeSlider->parentWidget()->setVisible(showBase);
    if (m_opacitySlider)  m_opacitySlider->parentWidget()->setVisible(showBase);
    if (m_minSizeRow)     m_minSizeRow->setVisible(showBase && hasMinCtrls);
    if (m_minOpacityRow)  m_minOpacityRow->setVisible(showBase && hasMinCtrls);
    if (m_hardnessRow)    m_hardnessRow->setVisible(showBase && hasHard);
    if (m_blendingRow)    m_blendingRow->setVisible(isWet);
    if (m_dilutionRow)    m_dilutionRow->setVisible(hasDilution);
    if (m_persistenceRow) m_persistenceRow->setVisible(isWet);
    if (m_blurPressureRow) m_blurPressureRow->setVisible(hasBlurPrs);
    if (m_coloringRow)    m_coloringRow->setVisible(isSmudge);
    if (m_uncolorRow)     m_uncolorRow->setVisible(isSmudge);
    if (m_blurWidthRow)   m_blurWidthRow->setVisible(isBlur);
    if (m_blendRow)       m_blendRow->setVisible(!isBucket);
}

void ToolbarPanel::updateShapeButtons()
{
    const TipType tt = m_settings.tipType;
    const bool hasShape = (tt == TipType::Pixel    || tt == TipType::Airbrush ||
                           tt == TipType::Brush    || tt == TipType::WaterColor ||
                           tt == TipType::Marker   || tt == TipType::Eraser);
    if (m_shapeRow)   m_shapeRow->setVisible(hasShape);
    if (m_textureRow) m_textureRow->setVisible(hasShape);
}

// ─────────────────────────────────────────────────────────────────────────────
// Colour helpers
// ─────────────────────────────────────────────────────────────────────────────

void ToolbarPanel::updateColorSquares()
{
    auto css = [](const QColor &c)
    {
        return QString("background-color:%1; border:1px solid #888;").arg(c.name());
    };
    if (m_primarySwatch)   m_primarySwatch->setStyleSheet(css(m_primary));
    if (m_secondarySwatch) m_secondarySwatch->setStyleSheet(css(m_secondary));
}

void ToolbarPanel::emitSettings()
{
    if (m_brushEngine)
        m_brushEngine->setSettings(m_settings);
    emit brushSettingsChanged(m_settings);
}

void ToolbarPanel::saveLastColor()
{
    QSettings settings("PixelCanvas", "PixelCanvas");
    settings.setValue("lastColor", m_primary.name(QColor::HexArgb));
    settings.sync();
}

void ToolbarPanel::onPrimaryColorClicked()
{
    const QColor c = QColorDialog::getColor(m_primary, nullptr, tr("Primary Colour"));
    if (c.isValid())
    {
        m_primary = c;
        if (m_colorWheel) m_colorWheel->setColor(c);
        updateColorSquares();
        emit colorChanged(m_primary);
        updatePreview();
        saveLastColor();
    }
}

void ToolbarPanel::onSwapColors()
{
    std::swap(m_primary, m_secondary);
    if (m_colorWheel) m_colorWheel->setColor(m_primary);
    updateColorSquares();
    emit colorChanged(m_primary);
    updatePreview();
    saveLastColor();
}

// ─────────────────────────────────────────────────────────────────────────────
// Slider slots  –  write to m_settings, save to slot, update preview
// ─────────────────────────────────────────────────────────────────────────────

void ToolbarPanel::onSizeChanged(int v)
{ m_settings.size            = (float)v;      saveUIIntoSlot(m_activeSlot); updatePreview(); emitSettings(); }

void ToolbarPanel::onMinSizeChanged(int v)
{ m_settings.minSizeFraction = v / 100.0f;    saveUIIntoSlot(m_activeSlot); updatePreview(); emitSettings(); }

void ToolbarPanel::onOpacityChanged(int v)
{ m_settings.opacity         = v / 100.0f;    saveUIIntoSlot(m_activeSlot); updatePreview(); emitSettings(); }

void ToolbarPanel::onMinOpacityChanged(int v)
{ m_settings.minOpacity      = v / 100.0f;    saveUIIntoSlot(m_activeSlot); updatePreview(); emitSettings(); }

void ToolbarPanel::onHardnessChanged(int v)
{ m_settings.hardness        = v / 100.0f;    saveUIIntoSlot(m_activeSlot); updatePreview(); emitSettings(); }

void ToolbarPanel::onBlendingChanged(int v)
{ m_settings.blending        = v / 100.0f;    saveUIIntoSlot(m_activeSlot); updatePreview(); emitSettings(); }

void ToolbarPanel::onDilutionChanged(int v)
{ m_settings.dilution        = v / 100.0f;    saveUIIntoSlot(m_activeSlot); updatePreview(); emitSettings(); }

void ToolbarPanel::onPersistenceChanged(int v)
{ m_settings.persistence     = v / 100.0f;    saveUIIntoSlot(m_activeSlot); updatePreview(); emitSettings(); }

void ToolbarPanel::onBlurPressureChanged(int v)
{ m_settings.blurPressure    = v / 100.0f;    saveUIIntoSlot(m_activeSlot); updatePreview(); emitSettings(); }

void ToolbarPanel::onColoringChanged(int v)
{ m_settings.coloring        = v / 100.0f;    saveUIIntoSlot(m_activeSlot); updatePreview(); emitSettings(); }

void ToolbarPanel::onUncolorPressureChanged(int v)
{ m_settings.uncolorPressure = v / 100.0f;    saveUIIntoSlot(m_activeSlot); updatePreview(); emitSettings(); }

void ToolbarPanel::onBlurWidthChanged(int v)
{ m_settings.blurWidth       = v / 100.0f;    saveUIIntoSlot(m_activeSlot); updatePreview(); emitSettings(); }

void ToolbarPanel::onBlendModeChanged(int index)
{
    static const BrushBlendMode modes[] = {
        BrushBlendMode::Normal,    BrushBlendMode::Multiply,
        BrushBlendMode::Screen,    BrushBlendMode::Overlay,
        BrushBlendMode::Luminosity, BrushBlendMode::Shade,
        BrushBlendMode::Vivid,     BrushBlendMode::Deep,
    };
    if (index >= 0 && index < (int)std::size(modes))
        m_settings.blendMode = modes[index];
    saveUIIntoSlot(m_activeSlot);
    emitSettings();
}

// ─────────────────────────────────────────────────────────────────────────────
// Tool selection  (keyboard shortcuts B / E)
// ─────────────────────────────────────────────────────────────────────────────

void ToolbarPanel::selectTool(Tool t)
{
    m_activeTool = t;
    const TipType target = (t == Tool::Eraser) ? TipType::Eraser : TipType::Pixel;
    for (int i = 0; i < kTotalSlots; ++i)
    {
        if (!m_slots[i].empty && m_slots[i].settings.tipType == target)
        {
            selectSlot(i);
            break;
        }
    }
    emit toolChanged(t);
}

void ToolbarPanel::onExternalColorChanged(const QColor &color)
{
    m_primary = color;
    if (m_colorWheel) m_colorWheel->setColor(color);
    updateColorSquares();
    emit colorChanged(m_primary);
    updatePreview();
    saveLastColor();
}

// ─────────────────────────────────────────────────────────────────────────────
// Settings persistence
// ─────────────────────────────────────────────────────────────────────────────

void ToolbarPanel::saveSettings()
{
    QSettings cfg("PixelCanvas", "PixelCanvas");
    cfg.beginWriteArray("brushSlots");
    for (int i = 0; i < kTotalSlots; ++i)
    {
        cfg.setArrayIndex(i);
        const BrushSlot &sl = m_slots[i];
        cfg.setValue("empty", sl.empty);
        if (sl.empty) continue;

        cfg.setValue("name",        sl.name);
        cfg.setValue("shapePath",   sl.shapePath);
        cfg.setValue("texturePath", sl.texturePath);

        const BrushSettings &s = sl.settings;
        cfg.setValue("size",            s.size);
        cfg.setValue("sizeMultiplier",  s.sizeMultiplier);
        cfg.setValue("minSizeFraction", s.minSizeFraction);
        cfg.setValue("opacity",         s.opacity);
        cfg.setValue("minOpacity",      s.minOpacity);
        cfg.setValue("hardness",        s.hardness);
        cfg.setValue("spacing",         s.spacing);
        cfg.setValue("smoothing",       s.smoothing);
        cfg.setValue("blendMode",       (int)s.blendMode);
        cfg.setValue("keepOpacity",     s.keepOpacity);
        cfg.setValue("blending",        s.blending);
        cfg.setValue("dilution",        s.dilution);
        cfg.setValue("persistence",     s.persistence);
        cfg.setValue("blurPressure",    s.blurPressure);
        cfg.setValue("coloring",        s.coloring);
        cfg.setValue("uncolorPressure", s.uncolorPressure);
        cfg.setValue("blurWidth",       s.blurWidth);
        cfg.setValue("brushShape",      s.brushShape);
        cfg.setValue("textureStrength", s.textureStrength);
        cfg.setValue("tipType",         (int)s.tipType);
    }
    cfg.endArray();
    cfg.setValue("activeSlot", m_activeSlot);
}

void ToolbarPanel::loadSettings()
{
    QSettings cfg("PixelCanvas", "PixelCanvas");
    const int n = cfg.beginReadArray("brushSlots");
    for (int i = 0; i < n && i < kTotalSlots; ++i)
    {
        cfg.setArrayIndex(i);
        const bool empty = cfg.value("empty", true).toBool();

        // Never overwrite the built-in slots (0–9) with an empty record.
        if (empty && i < 10) continue;
        if (empty) { m_slots[i] = BrushSlot{}; continue; }

        BrushSlot &sl   = m_slots[i];
        sl.empty        = false;
        sl.name         = cfg.value("name").toString();
        sl.shapePath    = cfg.value("shapePath").toString();
        sl.texturePath  = cfg.value("texturePath").toString();

        BrushSettings &s  = sl.settings;
        s.size            = cfg.value("size",            s.size).toFloat();
        s.sizeMultiplier  = cfg.value("sizeMultiplier",  s.sizeMultiplier).toFloat();
        s.minSizeFraction = cfg.value("minSizeFraction", s.minSizeFraction).toFloat();
        s.opacity         = cfg.value("opacity",         s.opacity).toFloat();
        s.minOpacity      = cfg.value("minOpacity",      s.minOpacity).toFloat();
        s.hardness        = cfg.value("hardness",        s.hardness).toFloat();
        s.spacing         = cfg.value("spacing",         s.spacing).toFloat();
        s.smoothing       = cfg.value("smoothing",       s.smoothing).toFloat();
        s.blendMode       = (BrushBlendMode)cfg.value("blendMode", (int)s.blendMode).toInt();
        s.keepOpacity     = cfg.value("keepOpacity",     s.keepOpacity).toBool();
        s.blending        = cfg.value("blending",        s.blending).toFloat();
        s.dilution        = cfg.value("dilution",        s.dilution).toFloat();
        s.persistence     = cfg.value("persistence",     s.persistence).toFloat();
        s.blurPressure    = cfg.value("blurPressure",    s.blurPressure).toFloat();
        s.coloring        = cfg.value("coloring",        s.coloring).toFloat();
        s.uncolorPressure = cfg.value("uncolorPressure", s.uncolorPressure).toFloat();
        s.blurWidth       = cfg.value("blurWidth",       s.blurWidth).toFloat();
        s.brushShape      = cfg.value("brushShape",      s.brushShape).toInt();
        s.textureStrength = cfg.value("textureStrength", s.textureStrength).toFloat();
        s.tipType         = (TipType)cfg.value("tipType", (int)s.tipType).toInt();

        // Built-in shapes 1 (diamond) and 2 (square) never use a BMP path.
        if (s.brushShape == 1 || s.brushShape == 2)
            sl.shapePath = {};
    }
    cfg.endArray();

    const int active = cfg.value("activeSlot", 0).toInt();
    if (active >= 0 && active < kTotalSlots && !m_slots[active].empty)
        m_activeSlot = active;
}
