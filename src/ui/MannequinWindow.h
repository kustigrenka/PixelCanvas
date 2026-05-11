#pragma once

#include <QDialog>
#include <QEvent>
#include <QPoint>
#include <QSize>

class QLineEdit;

#ifdef QT_WEBENGINEWIDGETS_LIB
class QWebEngineView;
#endif

// ─────────────────────────────────────────────────────────────────────────────
// MannequinWindow  –  floating pose-reference panel (PoseMy.Art)
//
// Frameless QDialog with a slim custom title bar and URL bar.
// Embeds QWebEngineView when available; falls back to a plain "Open in
// browser" button otherwise.
// ─────────────────────────────────────────────────────────────────────────────

class MannequinWindow : public QDialog
{
    Q_OBJECT

public:
    explicit MannequinWindow(QWidget *parent  = nullptr,
                             const QString &url = QString());

protected:
    void mousePressEvent  (QMouseEvent *e) override;
    void mouseMoveEvent   (QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    bool eventFilter(QObject *obj, QEvent *e) override;

private:
    void buildUI(const QString &url);
    void navigate(const QString &url);

    QLineEdit *m_urlBar = nullptr;

    // ── Drag-to-move state ────────────────────────────────────────────────────
    bool   m_dragging  = false;
    QPoint m_dragOrigin;

    // ── Resize-grip state ─────────────────────────────────────────────────────
    bool  m_resizing        = false;
    QPoint m_resizeStart;
    QSize  m_resizeStartSize;

#ifdef QT_WEBENGINEWIDGETS_LIB
    QWebEngineView *m_webView = nullptr;
#endif
};
