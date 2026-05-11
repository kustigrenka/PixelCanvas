// ─────────────────────────────────────────────────────────────────────────────
// MannequinWindow.cpp  –  floating pose-reference panel (PoseMy.Art)
// ─────────────────────────────────────────────────────────────────────────────
#include "MannequinWindow.h"
#include "MannequinWindow.moc"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QToolButton>
#include <QMouseEvent>
#include <QScreen>
#include <QGuiApplication>
#include <QDesktopServices>
#include <QEvent>
#include <QUrl>
#include <QApplication>
#include <algorithm>

#ifdef QT_WEBENGINEWIDGETS_LIB
#  include <QWebEngineView>
#  include <QWebEngineProfile>
#  include <QWebEngineSettings>
#  include <QWebEnginePage>
#  include <QtWebEngineWidgets>
#endif

static constexpr char kDefaultUrl[] = "https://posemy.art/";
static constexpr int  kPanelWidth   = 480;

// ─────────────────────────────────────────────────────────────────────────────
// Construction
// ─────────────────────────────────────────────────────────────────────────────

MannequinWindow::MannequinWindow(QWidget *parent, const QString &url)
    : QDialog(parent, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint)
{
    setAttribute(Qt::WA_DeleteOnClose, false);
    setObjectName("MannequinWindow");

    buildUI(url);

    QRect screen = QGuiApplication::primaryScreen()->availableGeometry();
    if (parent)
    {
        const QRect pw = parent->geometry();
        screen = QRect(pw.left(), pw.top(), pw.width(), pw.height());
    }
    const int h = screen.height() - 40;
    setGeometry(screen.right() - kPanelWidth - 4,
                screen.top()  + 20,
                kPanelWidth,   h);
}

// ─────────────────────────────────────────────────────────────────────────────
// UI construction
// ─────────────────────────────────────────────────────────────────────────────

void MannequinWindow::buildUI(const QString &url)
{
    setStyleSheet("QDialog { background:#1e1e1e; border:1px solid #111; }");
    setMinimumSize(250, 300);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── Title bar ─────────────────────────────────────────────────────────────
    auto *titleBar = new QWidget(this);
    titleBar->setFixedHeight(34);
    titleBar->setStyleSheet(
        "QWidget { background:#1a5276; border-bottom:1px solid #0d2840; }");

    auto *tbLayout = new QHBoxLayout(titleBar);
    tbLayout->setContentsMargins(8, 0, 4, 0);
    tbLayout->setSpacing(4);

    auto *pinIcon = new QLabel("◈", titleBar);
    pinIcon->setStyleSheet(
        "QLabel { color:#9ab8e8; font-size:16px; background:transparent; border:none; }");
    tbLayout->addWidget(pinIcon);

    auto *titleLbl = new QLabel("Pose Reference", titleBar);
    titleLbl->setStyleSheet(
        "QLabel { color:#fff; font-weight:bold; font-size:13px; background:transparent; border:none; }");
    tbLayout->addWidget(titleLbl, 1);

    auto *closeBtn = new QToolButton(titleBar);
    closeBtn->setText("✕");
    closeBtn->setFixedSize(26, 26);
    closeBtn->setStyleSheet(
        "QToolButton { background:transparent; color:#fff; border:none; font-size:13px; border-radius:4px; }"
        "QToolButton:hover { background:#e74c3c; }");
    connect(closeBtn, &QToolButton::clicked, this, &QDialog::hide);
    tbLayout->addWidget(closeBtn);

    root->addWidget(titleBar);

    // ── Navigation bar ────────────────────────────────────────────────────────
    auto *navBar = new QWidget(this);
    navBar->setFixedHeight(32);
    navBar->setStyleSheet(
        "QWidget { background:#1a1a1a; border-bottom:1px solid #111; }");

    auto *navLayout = new QHBoxLayout(navBar);
    navLayout->setContentsMargins(6, 3, 6, 3);
    navLayout->setSpacing(4);

    const QString navBtnStyle =
        "QToolButton { background:#2b2b2b; color:#d0d0d0; border:1px solid #444; border-radius:3px; font-size:15px; }"
        "QToolButton:hover { background:#3a3a3a; }";

#ifdef QT_WEBENGINEWIDGETS_LIB
    auto *backBtn = new QToolButton(navBar);
    backBtn->setText("‹");
    backBtn->setFixedSize(22, 22);
    backBtn->setStyleSheet(navBtnStyle);
    navLayout->addWidget(backBtn);

    auto *fwdBtn = new QToolButton(navBar);
    fwdBtn->setText("›");
    fwdBtn->setFixedSize(22, 22);
    fwdBtn->setStyleSheet(navBtnStyle);
    navLayout->addWidget(fwdBtn);

    auto *homeBtn = new QToolButton(navBar);
    homeBtn->setText("⌂");
    homeBtn->setFixedSize(22, 22);
    homeBtn->setToolTip("PoseMy.Art home");
    homeBtn->setStyleSheet(navBtnStyle);
    connect(homeBtn, &QToolButton::clicked, this, [this]() { navigate(kDefaultUrl); });
    navLayout->addWidget(homeBtn);
#endif

    m_urlBar = new QLineEdit(url.isEmpty() ? QString(kDefaultUrl) : url, navBar);
    m_urlBar->setStyleSheet(
        "QLineEdit { background:#2b2b2b; color:#d0d0d0; border:1px solid #444; "
        "border-radius:3px; padding:2px 6px; font-size:11px; }");
    connect(m_urlBar, &QLineEdit::returnPressed, this, [this]() { navigate(m_urlBar->text()); });
    navLayout->addWidget(m_urlBar, 1);

    auto *goBtn = new QToolButton(navBar);
    goBtn->setText("→");
    goBtn->setFixedSize(24, 24);
    goBtn->setStyleSheet(
        "QToolButton { background:#1a5276; color:#fff; border:none; border-radius:3px; font-size:13px; }"
        "QToolButton:hover { background:#2980b9; }");
    connect(goBtn, &QToolButton::clicked, this, [this]() { navigate(m_urlBar->text()); });
    navLayout->addWidget(goBtn);

    root->addWidget(navBar);

    // ── Web content ───────────────────────────────────────────────────────────
#ifdef QT_WEBENGINEWIDGETS_LIB
    m_webView = new QWebEngineView(this);

    auto *profile = new QWebEngineProfile("posemyart_profile", m_webView);
    profile->setPersistentCookiesPolicy(QWebEngineProfile::AllowPersistentCookies);
    profile->setHttpUserAgent(
        "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 "
        "(KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36");

    auto *page = new QWebEnginePage(profile, m_webView);
    m_webView->setPage(page);

    m_webView->settings()->setAttribute(QWebEngineSettings::JavascriptEnabled,        true);
    m_webView->settings()->setAttribute(QWebEngineSettings::LocalStorageEnabled,      true);
    m_webView->settings()->setAttribute(QWebEngineSettings::ScrollAnimatorEnabled,    true);
    m_webView->settings()->setAttribute(QWebEngineSettings::PluginsEnabled,           true);
    m_webView->settings()->setAttribute(QWebEngineSettings::JavascriptCanOpenWindows, true);

    m_webView->setUrl(QUrl(url.isEmpty() ? QString(kDefaultUrl) : url));

    // Redirect "open in new tab/window" back into the same view.
    connect(page, &QWebEnginePage::newWindowRequested, this,
            [this](QWebEngineNewWindowRequest &req)
    {
        m_webView->setUrl(req.requestedUrl());
        req.openIn(m_webView->page());
    });

    connect(m_webView, &QWebEngineView::urlChanged, this, [this](const QUrl &u) {
        m_urlBar->setText(u.toString());
    });

    connect(backBtn, &QToolButton::clicked, m_webView, &QWebEngineView::back);
    connect(fwdBtn,  &QToolButton::clicked, m_webView, &QWebEngineView::forward);

    root->addWidget(m_webView, 1);

#else
    // ── Fallback (no WebEngine) ───────────────────────────────────────────────
    auto *fallbackWidget = new QWidget(this);
    fallbackWidget->setStyleSheet("QWidget { background:#1e1e1e; }");
    auto *fl = new QVBoxLayout(fallbackWidget);
    fl->setContentsMargins(24, 40, 24, 24);
    fl->setSpacing(16);
    fl->addStretch(1);

    auto *icon = new QLabel("◈", fallbackWidget);
    icon->setAlignment(Qt::AlignCenter);
    icon->setStyleSheet("QLabel { color:#1a5276; font-size:48px; background:transparent; border:none; }");
    fl->addWidget(icon);

    auto *heading = new QLabel("Open Pose Reference", fallbackWidget);
    heading->setAlignment(Qt::AlignCenter);
    heading->setStyleSheet(
        "QLabel { color:#d0d0d0; font-size:15px; font-weight:bold; background:transparent; border:none; }");
    fl->addWidget(heading);

    auto *desc = new QLabel(
        "Qt WebEngine is not available.\nClick below to open PoseMy.Art in your browser.",
        fallbackWidget);
    desc->setAlignment(Qt::AlignCenter);
    desc->setWordWrap(true);
    desc->setStyleSheet(
        "QLabel { color:#888; font-size:11px; background:transparent; border:none; }");
    fl->addWidget(desc);

    auto *openBtn = new QToolButton(fallbackWidget);
    openBtn->setText("Open posemy.art  ↗");
    openBtn->setFixedHeight(36);
    openBtn->setStyleSheet(
        "QToolButton { background:#1a5276; color:#fff; border:none; border-radius:5px; "
        "font-size:12px; font-weight:bold; padding:0 16px; }"
        "QToolButton:hover { background:#2980b9; }");
    connect(openBtn, &QToolButton::clicked, this, []()
    {
        QDesktopServices::openUrl(QUrl(kDefaultUrl));
    });

    auto *btnRow = new QHBoxLayout;
    btnRow->addStretch(1);
    btnRow->addWidget(openBtn);
    btnRow->addStretch(1);
    fl->addLayout(btnRow);
    fl->addStretch(2);
    root->addWidget(fallbackWidget, 1);

    // Resize grip
    auto *grip = new QWidget(this);
    grip->setFixedHeight(8);
    grip->setStyleSheet("QWidget { background:#111; border-top:1px solid #333; }");
    grip->setCursor(Qt::SizeFDiagCursor);
    grip->installEventFilter(this);
    grip->setObjectName("resizeGrip");
    root->addWidget(grip);
#endif
}

// ─────────────────────────────────────────────────────────────────────────────
// Navigation
// ─────────────────────────────────────────────────────────────────────────────

void MannequinWindow::navigate(const QString &url)
{
    QString target = url.trimmed();
    if (!target.startsWith("http://") && !target.startsWith("https://"))
        target = "https://posemy.art/search?q="
                 + QString::fromUtf8(QUrl::toPercentEncoding(target));

#ifdef QT_WEBENGINEWIDGETS_LIB
    if (m_webView) m_webView->setUrl(QUrl(target));
#else
    QDesktopServices::openUrl(QUrl(target));
#endif
    m_urlBar->setText(target);
}

// ─────────────────────────────────────────────────────────────────────────────
// Mouse events  (drag-to-move via title bar, resize via grip)
// ─────────────────────────────────────────────────────────────────────────────

void MannequinWindow::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton && e->position().y() < 34)
    {
        m_dragging   = true;
        m_dragOrigin = e->globalPosition().toPoint() - frameGeometry().topLeft();
        e->accept();
    }
    else
    {
        QDialog::mousePressEvent(e);
    }
}

void MannequinWindow::mouseMoveEvent(QMouseEvent *e)
{
    if (m_dragging)
    {
        move(e->globalPosition().toPoint() - m_dragOrigin);
        e->accept();
    }
    else
    {
        QDialog::mouseMoveEvent(e);
    }
}

void MannequinWindow::mouseReleaseEvent(QMouseEvent *e)
{
    m_dragging = false;
    QDialog::mouseReleaseEvent(e);
}

bool MannequinWindow::eventFilter(QObject *obj, QEvent *e)
{
    if (obj->objectName() == "resizeGrip")
    {
        if (e->type() == QEvent::MouseButtonPress)
        {
            auto *me = static_cast<QMouseEvent *>(e);
            if (me->button() == Qt::LeftButton)
            {
                m_resizing        = true;
                m_resizeStart     = me->globalPosition().toPoint();
                m_resizeStartSize = size();
                return true;
            }
        }
        else if (e->type() == QEvent::MouseMove && m_resizing)
        {
            auto *me    = static_cast<QMouseEvent *>(e);
            QPoint delta = me->globalPosition().toPoint() - m_resizeStart;
            resize(std::max(250, m_resizeStartSize.width()  + delta.x()),
                   std::max(300, m_resizeStartSize.height() + delta.y()));
            return true;
        }
        else if (e->type() == QEvent::MouseButtonRelease)
        {
            m_resizing = false;
            return true;
        }
    }
    return QDialog::eventFilter(obj, e);
}
