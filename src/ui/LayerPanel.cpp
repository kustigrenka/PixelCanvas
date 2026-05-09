#include "LayerPanel.h"
#include "LayerStack.h"
#include "Layer.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QListWidget>
#include <QSlider>
#include <QSpinBox>
#include <QComboBox>
#include <QPushButton>
#include <QToolButton>
#include <QLabel>
#include <QFrame>

// Blend mode <-> combo index mapping (must match addItems order in buildUi)
static const QPainter::CompositionMode kBlendModes[] = {
    QPainter::CompositionMode_SourceOver,  // Normal
    QPainter::CompositionMode_Multiply,    // Multiply
    QPainter::CompositionMode_Screen,      // Screen
    QPainter::CompositionMode_Overlay,     // Overlay
    QPainter::CompositionMode_Plus,        // Add
    QPainter::CompositionMode_ColorDodge,  // Luminosity (closest Qt equivalent)
};
static constexpr int kBlendModeCount = static_cast<int>(std::size(kBlendModes));

LayerPanel::LayerPanel(LayerStack *layerStack, QWidget *parent)
    : QWidget(parent)
    , m_layerStack(layerStack)
{
    buildUi();
    connect(m_layerStack, &LayerStack::layersChanged,      this, &LayerPanel::refresh);
    connect(m_layerStack, &LayerStack::activeLayerChanged, this, &LayerPanel::refresh);
    refresh();
}

QFrame *LayerPanel::buildSeparator()
{
    auto *f = new QFrame(this);
    f->setFrameShape(QFrame::HLine);
    f->setFrameShadow(QFrame::Sunken);
    return f;
}

void LayerPanel::buildUi()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(4, 4, 4, 4);
    root->setSpacing(4);

    // ── Blend mode ────────────────────────────────────────────────────────────
    root->addWidget(new QLabel(tr("Blend Mode:"), this));
    m_blendCombo = new QComboBox(this);
    m_blendCombo->addItems({"Normal", "Multiply", "Screen", "Overlay",
                             "Add", "Luminosity"});
    connect(m_blendCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &LayerPanel::onBlendModeChanged);
    root->addWidget(m_blendCombo);

    // ── Opacity ───────────────────────────────────────────────────────────────
    root->addWidget(new QLabel(tr("Opacity:"), this));
    auto *opRow = new QHBoxLayout;
    m_opacitySlider = new QSlider(Qt::Horizontal, this);
    m_opacitySlider->setRange(0, 100);
    m_opacitySlider->setValue(100);
    m_opacitySpin = new QSpinBox(this);
    m_opacitySpin->setRange(0, 100);
    m_opacitySpin->setValue(100);
    m_opacitySpin->setSuffix("%");
    m_opacitySpin->setFixedWidth(54);
    connect(m_opacitySlider, &QSlider::valueChanged,
            m_opacitySpin,   &QSpinBox::setValue);
    connect(m_opacitySpin, QOverload<int>::of(&QSpinBox::valueChanged),
            m_opacitySlider, &QSlider::setValue);
    connect(m_opacitySlider, &QSlider::valueChanged,
            this, &LayerPanel::onOpacityChanged);
    opRow->addWidget(m_opacitySlider);
    opRow->addWidget(m_opacitySpin);
    root->addLayout(opRow);

    root->addWidget(buildSeparator());

    // ── Layer list ────────────────────────────────────────────────────────────
    m_layerList = new QListWidget(this);
    m_layerList->setDragDropMode(QAbstractItemView::InternalMove);
    m_layerList->setSelectionMode(QAbstractItemView::SingleSelection);
    connect(m_layerList, &QListWidget::currentRowChanged,
            this, &LayerPanel::onLayerSelected);
    // Wire drag-reorder → LayerStack
    connect(m_layerList->model(), &QAbstractItemModel::rowsMoved,
            this, &LayerPanel::onRowsMoved);
    root->addWidget(m_layerList, 1);  // stretch

    // ── Action buttons ────────────────────────────────────────────────────────
    auto *btnRow = new QHBoxLayout;
    btnRow->setSpacing(2);

    m_addBtn = new QPushButton("+", this);
    m_addBtn->setFixedWidth(28);
    m_addBtn->setToolTip(tr("New layer"));

    m_removeBtn = new QPushButton("−", this);
    m_removeBtn->setFixedWidth(28);
    m_removeBtn->setToolTip(tr("Delete layer"));

    m_dupBtn = new QPushButton("⧉", this);
    m_dupBtn->setFixedWidth(28);
    m_dupBtn->setToolTip(tr("Duplicate layer"));

    m_visBtn = new QToolButton(this);
    m_visBtn->setText("👁");
    m_visBtn->setCheckable(true);
    m_visBtn->setChecked(true);
    m_visBtn->setToolTip(tr("Toggle visibility"));
    m_visBtn->setFixedWidth(28);

    connect(m_addBtn,    &QPushButton::clicked,  this, &LayerPanel::onAddLayer);
    connect(m_removeBtn, &QPushButton::clicked,  this, &LayerPanel::onRemoveLayer);
    connect(m_dupBtn,    &QPushButton::clicked,  this, &LayerPanel::onDuplicateLayer);
    connect(m_visBtn,    &QToolButton::clicked,  this, &LayerPanel::onVisibilityToggled);

    btnRow->addWidget(m_addBtn);
    btnRow->addWidget(m_removeBtn);
    btnRow->addWidget(m_dupBtn);
    btnRow->addWidget(m_visBtn);
    btnRow->addStretch();
    root->addLayout(btnRow);

    setMinimumWidth(180);
}

// ─────────────────────────────────────────────────────────────────────────────
void LayerPanel::refresh()
{
    const QSignalBlocker b1(m_layerList);
    m_layerList->clear();

    // Layers displayed top-to-bottom = highest index first (same as SAI)
    for (int i = m_layerStack->count() - 1; i >= 0; --i)
    {
        Layer *layer = m_layerStack->layerAt(i);
        if (!layer) continue;

        const QString eyePrefix = layer->visible ? "👁 " : "   ";
        m_layerList->addItem(eyePrefix + layer->name);
    }

    // Sync selected row
    const int activeRow = m_layerStack->count() - 1 - m_layerStack->activeIndex();
    m_layerList->setCurrentRow(activeRow);

    // Sync opacity + blend + visibility widgets to active layer
    Layer *active = m_layerStack->activeLayer();
    if (active)
    {
        const QSignalBlocker b2(m_opacitySlider);
        const QSignalBlocker b3(m_opacitySpin);
        const int op = static_cast<int>(active->opacity * 100.0);
        m_opacitySlider->setValue(op);
        m_opacitySpin->setValue(op);

        m_visBtn->setChecked(active->visible);

        // Sync blend mode combo
        const QSignalBlocker b4(m_blendCombo);
        for (int i = 0; i < kBlendModeCount; ++i) {
            if (kBlendModes[i] == active->blendMode) {
                m_blendCombo->setCurrentIndex(i);
                break;
            }
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
void LayerPanel::onAddLayer()
{
    const QString name = QString("Layer %1").arg(m_layerStack->count() + 1);
    const int idx = m_layerStack->addLayer(name);
    m_layerStack->setActiveLayer(idx);
}

void LayerPanel::onRemoveLayer()
{
    m_layerStack->removeLayer(m_layerStack->activeIndex());
}

void LayerPanel::onDuplicateLayer()
{
    const int newIdx = m_layerStack->duplicateLayer(m_layerStack->activeIndex());
    if (newIdx >= 0)
        m_layerStack->setActiveLayer(newIdx);
}

void LayerPanel::onLayerSelected(int row)
{
    if (row < 0) return;
    const int layerIndex = m_layerStack->count() - 1 - row;
    m_layerStack->setActiveLayer(layerIndex);
}

void LayerPanel::onRowsMoved(const QModelIndex & /*parent*/,
                              int srcRow, int /*srcEnd*/,
                              const QModelIndex & /*dst*/, int dstRow)
{
    // QListWidget rows are top=high-index; convert back to stack indices
    const int count = m_layerStack->count();
    const int from  = count - 1 - srcRow;
    const int to    = count - 1 - (dstRow > srcRow ? dstRow - 1 : dstRow);
    m_layerStack->moveLayer(from, to);
    m_layerStack->setActiveLayer(to);
}

void LayerPanel::onOpacityChanged(int value)
{
    Layer *layer = m_layerStack->activeLayer();
    if (layer) {
        layer->opacity = value / 100.0;
        emit m_layerStack->layerPropertiesChanged();
    }
}

void LayerPanel::onBlendModeChanged(int index)
{
    if (index < 0 || index >= kBlendModeCount) return;
    Layer *layer = m_layerStack->activeLayer();
    if (layer) {
        layer->blendMode = kBlendModes[index];
        emit m_layerStack->layerPropertiesChanged();
    }
}

void LayerPanel::onVisibilityToggled()
{
    Layer *layer = m_layerStack->activeLayer();
    if (layer) {
        layer->visible = m_visBtn->isChecked();
        refresh();
        emit m_layerStack->layerPropertiesChanged();
    }
}
