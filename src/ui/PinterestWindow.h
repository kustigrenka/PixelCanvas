#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// PinterestWindow.h
//
// A frameless floating QDialog that embeds Pinterest in a QWebEngineView.
// It has a slim custom title bar (drag-to-move, close button, "⬡ Pinterest"
// label) and a URL bar so the user can also navigate freely.
//
// Falls back to a plain QLabel with instructions when Qt WebEngine is absent.
//
// Position: right edge of the parent (MainWindow), 360px wide, full height.
// ─────────────────────────────────────────────────────────────────────────────

#include <QDialog>

class QLineEdit;

#ifdef QT_WEBENGINEWIDGETS_LIB
class QWebEngineView;
#endif

// ─────────────────────────────────────────────────────────────────────────────
class PinterestWindow : public QDialog
{
    Q_OBJECT

public:
    explicit PinterestWindow(QWidget *parent = nullptr);

protected:
    void mousePressEvent  (QMouseEvent *e) override;
    void mouseMoveEvent   (QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    bool eventFilter(QObject *obj, QEvent *e) override;

private:
    void buildUI();
    void navigate(const QString &url);

    QLineEdit  *m_urlBar   = nullptr;
    bool        m_dragging    = false;
    QPoint      m_dragOrigin;
    bool        m_resizing    = false;
    QPoint      m_resizeStart;
    QSize       m_resizeStartSize;

#ifdef QT_WEBENGINEWIDGETS_LIB
    QWebEngineView *m_webView = nullptr;
#endif
};
