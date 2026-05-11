#include "LayerStack.h"
#include "Filter.h"

#include <QPainter>
#include <algorithm>

LayerStack::LayerStack(QObject *parent)
    : QObject(parent)
{}

LayerStack::~LayerStack()
{
    qDeleteAll(m_layers);
}

void LayerStack::init(const QSize &canvasSize)
{
    m_canvasSize = canvasSize;
    qDeleteAll(m_layers);
    m_layers.clear();
    addLayer("Background");
    m_layers[0]->pixels.fill(Qt::white);
    m_activeIndex = 0;
    emit layersChanged();
}

// ─────────────────────────────────────────────────────────────────────────────
// Layer management
// ─────────────────────────────────────────────────────────────────────────────

int LayerStack::addLayer(const QString &name)
{
    auto *layer   = new Layer;
    layer->name   = name;
    layer->pixels = QImage(m_canvasSize, QImage::Format_ARGB32_Premultiplied);
    layer->pixels.fill(Qt::transparent);

    m_layers.append(layer);
    emit layersChanged();
    return m_layers.size() - 1;
}

void LayerStack::removeLayer(int index)
{
    if (index < 0 || index >= m_layers.size() || m_layers.size() <= 1) return;

    delete m_layers.takeAt(index);
    m_activeIndex = std::clamp(m_activeIndex, 0, (int)m_layers.size() - 1);
    emit layersChanged();
}

int LayerStack::duplicateLayer(int index)
{
    if (index < 0 || index >= m_layers.size()) return -1;

    const Layer *src = m_layers[index];
    auto *dup        = new Layer;
    dup->pixels    = src->pixels.copy();
    dup->name      = src->name + " copy";
    dup->opacity   = src->opacity;
    dup->blendMode = src->blendMode;
    dup->visible   = src->visible;

    m_layers.insert(index + 1, dup);
    emit layersChanged();
    return index + 1;
}

void LayerStack::moveLayer(int from, int to)
{
    if (from == to) return;
    m_layers.move(from, to);
    emit layersChanged();
}

void LayerStack::setActiveLayer(int index)
{
    if (index < 0 || index >= m_layers.size()) return;
    m_activeIndex = index;
    emit activeLayerChanged(m_activeIndex);
}

Layer *LayerStack::activeLayer()
{
    return m_layers.isEmpty() ? nullptr : m_layers[m_activeIndex];
}

Layer *LayerStack::layerAt(int index)
{
    if (index < 0 || index >= m_layers.size()) return nullptr;
    return m_layers[index];
}

// ─────────────────────────────────────────────────────────────────────────────
// Canvas size operations
// ─────────────────────────────────────────────────────────────────────────────

void LayerStack::resizeCanvas(const QSize &newSize, const QPoint &anchor)
{
    for (Layer *layer : m_layers)
    {
        QImage newPixels(newSize, QImage::Format_ARGB32_Premultiplied);
        newPixels.fill(Qt::transparent);
        QPainter p(&newPixels);
        p.setCompositionMode(QPainter::CompositionMode_Source);
        p.drawImage(anchor, layer->pixels);
        layer->pixels = std::move(newPixels);
    }
    m_canvasSize = newSize;
    emit layersChanged();
}

void LayerStack::extendCanvas(int left, int top, int right, int bottom)
{
    const QSize newSize(m_canvasSize.width()  + left + right,
                        m_canvasSize.height() + top  + bottom);
    resizeCanvas(newSize, QPoint(left, top));
}

// ─────────────────────────────────────────────────────────────────────────────
// Compositing
// ─────────────────────────────────────────────────────────────────────────────

void LayerStack::recompositeRect(QImage &dest, const QRect &dirty)
{
    const QRect region = dirty.intersected(dest.rect());
    if (region.isEmpty()) return;

    QPainter p(&dest);

    // ── 1. Fill with canvas background (white) ────────────────────────────────
    // CompositionMode_Source fully overwrites dest pixels — including any alpha
    // left by a previous eraser stroke.  Without this, erased (transparent)
    // pixels composite onto stale dest content instead of the white background.
    p.setCompositionMode(QPainter::CompositionMode_Source);
    p.fillRect(region, Qt::white);

    // ── 2. Composite each visible layer in order ──────────────────────────────
    for (const Layer *layer : m_layers)
    {
        if (!layer->visible) continue;
        p.setOpacity(layer->opacity);
        p.setCompositionMode(layer->blendMode);
        p.drawImage(region.topLeft(), layer->pixels, region);
    }

    // Reset opacity so nothing downstream is affected.
    p.setOpacity(1.0);
}

// ─────────────────────────────────────────────────────────────────────────────
// Filter
// ─────────────────────────────────────────────────────────────────────────────

void LayerStack::applyFilterToActive(Filter *filter)
{
    Layer *layer = activeLayer();
    if (!layer || !filter) return;
    layer->pixels = filter->apply(layer->pixels);
}
