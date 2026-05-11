#pragma once

#include <QObject>
#include <QVector>
#include <QImage>
#include <QRect>
#include <QSize>

#include "Layer.h"

class Filter;

// ─────────────────────────────────────────────────────────────────────────────
// LayerStack  (Toma – Ph. 6)
//
// Owns all Layer objects and composites them into the CanvasWidget's QImage.
// recompositeRect() must NEVER composite the whole canvas on every pointer move.
// ─────────────────────────────────────────────────────────────────────────────

class LayerStack : public QObject
{
    Q_OBJECT

public:
    explicit LayerStack(QObject *parent = nullptr);
    ~LayerStack() override;

    // Initialise with a blank canvas (called from CanvasWidget::initializeGL).
    void init(const QSize &canvasSize);

    // ── Layer management  (called by LayerPanel / MainWindow) ─────────────────

    int   addLayer(const QString &name = "Layer");
    void  removeLayer(int index);
    int   duplicateLayer(int index);
    void  moveLayer(int from, int to);
    void  setActiveLayer(int index);

    int    activeIndex() const { return m_activeIndex; }
    Layer *activeLayer();
    Layer *layerAt(int index);
    int    count()       const { return m_layers.size(); }

    // ── Canvas size operations ────────────────────────────────────────────────

    void  resizeCanvas(const QSize &newSize, const QPoint &anchor = {0, 0});
    void  extendCanvas(int left, int top, int right, int bottom);
    QSize canvasSize() const { return m_canvasSize; }

    // ── Compositing ──────────────────────────────────────────────────────────

    void recompositeRect(QImage &dest, const QRect &dirty);

    // ── Filter  (applied by CanvasWidget::applyFilter) ───────────────────────

    void applyFilterToActive(Filter *filter);

signals:
    void layersChanged();
    void activeLayerChanged(int index);
    void layerPropertiesChanged();   // opacity / blend mode / visibility changed

private:
    QVector<Layer *> m_layers;
    int              m_activeIndex = 0;
    QSize            m_canvasSize;
};
