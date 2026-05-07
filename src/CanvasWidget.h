#pragma once

#include <QOpenGLWidget>
#include <QImage>
#include <QPointF>
#include <QRect>

#include "Stroke.h"

class LayerStack;
class BrushEngine;
class UndoStack;
class Filter;

// ─────────────────────────────────────────────────────────────────────────────
// CanvasWidget  (Toma)
//
// QOpenGLWidget subclass.  Owns:
//   - composited display image (m_composited)
//   - viewport transform (m_offset, m_zoom)
//   - all mouse / tablet event handling
//   - recompositeRect(dirty) for dirty-region updates
// ─────────────────────────────────────────────────────────────────────────────
class CanvasWidget : public QOpenGLWidget
{
    Q_OBJECT

public:
    explicit CanvasWidget(LayerStack *layerStack,
                          BrushEngine *brushEngine,
                          UndoStack   *undoStack,
                          QWidget     *parent = nullptr);
    ~CanvasWidget() override;

    // Apply a filter to the active layer (pushes undo snapshot first)
    void applyFilter(Filter *filter);

    // Re-initialise display image after a canvas size change (LayerStack already resized)
    void reinitCanvas();

    // Zoom / pan / rotate / flip API
    void setZoom(float zoom);
    float zoom() const { return m_zoom; }
    void  resetView();
    void  forceRecomposite();

    void  setRotation(float degrees);
    float rotation() const { return m_rotation; }

    void  setFlipH(bool flip);
    bool  flipH() const { return m_flipH; }

    // Navigator: read-only access to the composited image for thumbnail rendering
    const QImage &compositedImage() const { return m_composited; }

protected:
    // ── QOpenGLWidget lifecycle ───────────────────────────────────────────────
    void initializeGL()  override;
    void resizeGL(int w, int h) override;
    void paintGL()       override;

    // ── Input events ──────────────────────────────────────────────────────────
    void tabletEvent(QTabletEvent *e)      override;
    void mousePressEvent(QMouseEvent *e)   override;
    void mouseMoveEvent(QMouseEvent *e)    override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void wheelEvent(QWheelEvent *e)        override;
    void keyPressEvent(QKeyEvent *e)       override;
    void keyReleaseEvent(QKeyEvent *e)     override;
    void enterEvent(QEnterEvent *e)        override;
    void leaveEvent(QEvent *e)             override;

signals:
    void cursorMoved(QPointF canvasPos, float pressure);
    void zoomChanged(float zoom);
    void rotationChanged(float degrees);
    void flipHChanged(bool flipped);

private:
    QPointF widgetToCanvas(const QPointF &wp) const;
    void recompositeRect(const QRect &dirty);
    void pointerBegin(const QPointF &widgetPos, float pressure,
                      float tiltX, float tiltY, float rotation);
    void pointerUpdate(const QPointF &widgetPos, float pressure,
                       float tiltX, float tiltY, float rotation);
    void pointerEnd();

    // ── Owned objects ─────────────────────────────────────────────────────────
    LayerStack  *m_layerStack  = nullptr;
    BrushEngine *m_brushEngine = nullptr;
    UndoStack   *m_undoStack   = nullptr;

    QImage  m_composited;

    // ── Viewport ──────────────────────────────────────────────────────────────
    QPointF m_offset;
    float   m_zoom     = 1.0f;
    float   m_rotation = 0.0f;   // degrees, CCW positive
    bool    m_flipH    = false;

    // ── Drawing state ─────────────────────────────────────────────────────────
    bool    m_drawing         = false;
    bool    m_tabletInUse     = false;

    // ── Pan state ─────────────────────────────────────────────────────────────
    bool    m_panning         = false;
    bool    m_middlePanning   = false;  // true while middle mouse button is held
    QPointF m_panStartWidget;
    QPointF m_panStartOffset;

    // ── Keyboard modifiers ────────────────────────────────────────────────────
    bool    m_spaceHeld       = false;
    bool    m_altHeld         = false;

    // ── Brush cursor overlay ──────────────────────────────────────────────────
    QPointF m_cursorPos;               // last known widget-space cursor position
    bool    m_cursorOnCanvas  = false; // hide circle when cursor leaves widget
};
