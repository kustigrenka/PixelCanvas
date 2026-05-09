#pragma once
#include <QDialog>
#include <QEvent>
class QLineEdit;
#ifdef QT_WEBENGINEWIDGETS_LIB
class QWebEngineView;
#endif
class MannequinWindow : public QDialog {
    Q_OBJECT
public:
    explicit MannequinWindow(QWidget *parent = nullptr,
                             const QString &url = QString());
protected:
    void mousePressEvent  (QMouseEvent *e) override;
    void mouseMoveEvent   (QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    bool eventFilter(QObject *obj, QEvent *e) override;
private:
    void buildUI(const QString &url);
    void navigate(const QString &url);
    QLineEdit *m_urlBar          = nullptr;
    bool       m_dragging        = false;
    QPoint     m_dragOrigin;
    bool       m_resizing        = false;
    QPoint     m_resizeStart;
    QSize      m_resizeStartSize;
#ifdef QT_WEBENGINEWIDGETS_LIB
    QWebEngineView *m_webView = nullptr;
#endif
};
