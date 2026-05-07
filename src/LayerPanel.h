#pragma once

#include <QWidget>
#include <QFrame>
#include <QModelIndex>

class LayerStack;
class QListWidget;
class QSlider;
class QSpinBox;
class QComboBox;
class QPushButton;
class QToolButton;

class LayerPanel : public QWidget
{
    Q_OBJECT

public:
    explicit LayerPanel(LayerStack *layerStack, QWidget *parent = nullptr);
    void refresh();

private slots:
    void onAddLayer();
    void onRemoveLayer();
    void onDuplicateLayer();
    void onLayerSelected(int row);
    void onRowsMoved(const QModelIndex &parent, int srcRow, int srcEnd,
                     const QModelIndex &dst, int dstRow);
    void onOpacityChanged(int value);
    void onBlendModeChanged(int index);
    void onVisibilityToggled();

private:
    void buildUi();
    QFrame *buildSeparator();

    LayerStack   *m_layerStack      = nullptr;
    QListWidget  *m_layerList       = nullptr;
    QSlider      *m_opacitySlider   = nullptr;
    QSpinBox     *m_opacitySpin     = nullptr;
    QComboBox    *m_blendCombo      = nullptr;
    QPushButton  *m_addBtn          = nullptr;
    QPushButton  *m_removeBtn       = nullptr;
    QPushButton  *m_dupBtn          = nullptr;
    QToolButton  *m_visBtn          = nullptr;
};
