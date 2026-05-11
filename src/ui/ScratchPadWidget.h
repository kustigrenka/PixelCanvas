#pragma once

#include <QWidget>
#include <QImage>
#include <QPointF>
#include <QSize>

class BrushEngine;

// ─────────────────────────────────────────────────────────────────────────────
// ScratchPadWidget  –  mini canvas using the shared BrushEngine
//
// Canvas resize policy (Fix 3):
//   resizeEvent() no longer wipes m_canvas.  Content is composited onto a
//   fresh background only when the widget actually grows, preserving all
//   strokes across accordion open/close animations (which generate many
//   resize events while maximumHeight is animated from 0 to its natural
//   height).
// ─────────────────────────────────────────────────────────────────────────────

class ScratchPadWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ScratchPadWidget(QWidget *parent = nullptr);
    ~ScratchPadWidget() override;

    void setBrushEngine(BrushEngine *engine);
    void setColor(const QColor &c);

    QImage image()           const { return m_canvas; }
    void   setImage(const QImage &img);
    void   clear();

signals:
    void colorPicked(const QColor &c);

protected:
    void paintEvent(QPaintEvent *)        override;
    void resizeEvent(QResizeEvent *)      override;
    void tabletEvent(QTabletEvent *e)     override;
    void mousePressEvent(QMouseEvent *e)  override;
    void mouseMoveEvent(QMouseEvent *e)   override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void enterEvent(QEnterEvent *e)       override;
    void leaveEvent(QEvent *e)            override;

private:
    void ensureCanvas();
    void pointerBegin (const QPointF &pos, float pressure);
    void pointerUpdate(const QPointF &pos, float pressure);
    void pointerEnd   ();
    void pickColor    (const QPointF &pos);

    // ── Canvas ────────────────────────────────────────────────────────────────
    QImage m_canvas;
    QSize  m_canvasSize;          // size at which m_canvas was last allocated

    // ── Engine / colour ───────────────────────────────────────────────────────
    BrushEngine *m_engine     = nullptr;
    QColor       m_color      = Qt::black;
    QImage      *m_savedLayer = nullptr;   // layer pointer saved/restored around strokes

    // ── Drawing state ─────────────────────────────────────────────────────────
    bool    m_drawing     = false;
    bool    m_tabletInUse = false;
    bool    m_altHeld     = false;
    bool    m_cursorOnPad = false;
    QPointF m_cursorPos;
};
