#pragma once

#include <QOpenGLWidget>
#include <QImage>
#include <QPointF>
#include <QRect>
#include <QStack>
#include <QTimer>

#include "Stroke.h"
#include "StrokeRecorder.h"

class LayerStack;
class BrushEngine;
class UndoStack;
class Filter;

class CanvasWidget : public QOpenGLWidget
{
    Q_OBJECT

public:
    explicit CanvasWidget(LayerStack *layerStack,
                          BrushEngine *brushEngine,
                          UndoStack   *undoStack,
                          QWidget     *parent = nullptr);
    ~CanvasWidget() override;

    void applyFilter(Filter *filter);
    void reinitCanvas();

    void  setZoom(float zoom);
    float zoom() const { return m_zoom; }
    void  resetView();
    void  forceRecomposite();

    void  setRotation(float degrees);
    float rotation() const { return m_rotation; }

    void  setFlipH(bool flip);
    bool  flipH() const { return m_flipH; }

    const QImage &compositedImage() const { return m_composited; }

    bool isDirty() const { return m_dirty; }
    void clearDirty()    { m_dirty = false; }

protected:
    void initializeGL()  override;
    void resizeGL(int w, int h) override;
    void paintGL()       override;

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
    void canvasUpdated();
    void colorPicked(const QColor &color);

private:

    bool    m_shiftHeld      = false;
    QPointF m_shiftLineStart;   // canvas-space anchor set when shift is first pressed
    bool    m_shiftLineActive = false;
    bool m_dirty = false;
    QPointF widgetToCanvas(const QPointF &wp) const;
    void recompositeRect(const QRect &dirty);
    void flushPendingDirty();          // called by m_repaintTimer
    void pointerBegin(const QPointF &widgetPos, float pressure,
                      float tiltX, float tiltY, float rotation);
    void pointerUpdate(const QPointF &widgetPos, float pressure,
                       float tiltX, float tiltY, float rotation);
    void pointerEnd();

    void doBucketFill(const QPointF &canvasPos);

    // ── Owned objects ─────────────────────────────────────────────────────────
    LayerStack  *m_layerStack  = nullptr;
    BrushEngine *m_brushEngine = nullptr;
    UndoStack   *m_undoStack   = nullptr;

    QImage  m_composited;

    // ── Viewport ──────────────────────────────────────────────────────────────
    QPointF m_offset;
    float   m_zoom     = 1.0f;
    float   m_rotation = 0.0f;
    bool    m_flipH    = false;

    // ── Drawing state ─────────────────────────────────────────────────────────
    bool    m_drawing       = false;
    bool    m_tabletInUse   = false;

    // ── Batched recomposite (avoids one composite per dab during fast strokes) ─
    QRect   m_pendingDirty;
    QTimer *m_repaintTimer  = nullptr;   // fires every 8 ms (~120 fps cap)

    // ── Pan state ─────────────────────────────────────────────────────────────
    bool    m_panning       = false;
    bool    m_middlePanning = false;
    QPointF m_panStartWidget;
    QPointF m_panStartOffset;

    // ── Keyboard modifiers ────────────────────────────────────────────────────
    bool    m_spaceHeld     = false;
    bool    m_altHeld       = false;

    // ── Brush cursor overlay ──────────────────────────────────────────────────
    QPointF m_cursorPos;
    bool    m_cursorOnCanvas = false;

    // ── Gradient / selection tools ────────────────────────────────────────────
    QPointF m_gradientStart;
    QImage  m_selectionMask;        // Grayscale8, same size as canvas
    bool    m_hasSelection = false;

    StrokeRecorder m_recorder;
    bool           m_recordingEnabled = false;
    public:
    void startRecording() { m_recorder.startRecording(); m_recordingEnabled = true; }
    void stopRecording()  { m_recorder.stopRecording();  m_recordingEnabled = false; }
    bool isRecording() const { return m_recordingEnabled; }
    const StrokeRecorder& recorder() const { return m_recorder; }
};
