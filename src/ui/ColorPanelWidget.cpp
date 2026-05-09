#include "ColorPanelWidget.h"

#include "ColorWheelWidget.h"
#include "ColorSlidersWidget.h"
#include "ColorSwatchWidget.h"
#include "ScratchPadWidget.h"
#include "BrushEngine.h"

#include <QVBoxLayout>
#include <QToolButton>
#include <QFrame>
#include <QPropertyAnimation>
#include <QDockWidget>
#include <QScrollArea>

// ─────────────────────────────────────────────────────────────────────────────
// AccordionSection
// ─────────────────────────────────────────────────────────────────────────────
class AccordionSection : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(int contentHeight READ contentHeight WRITE setContentHeight)

public:
    AccordionSection(const QString &title, QWidget *content,
                     bool startOpen, QWidget *parent = nullptr)
        : QWidget(parent), m_content(content)
    {
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

        auto *lay = new QVBoxLayout(this);
        lay->setContentsMargins(0, 0, 0, 0);
        lay->setSpacing(0);

        // ── Header ────────────────────────────────────────────────────────────
        m_btn = new QToolButton(this);
        m_btn->setCheckable(true);
        m_btn->setChecked(startOpen);
        m_btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        m_btn->setFixedHeight(24);
        m_btn->setToolButtonStyle(Qt::ToolButtonTextOnly);
        m_btn->setArrowType(startOpen ? Qt::DownArrow : Qt::RightArrow);
        m_btn->setText(title);
        m_btn->setStyleSheet(
            "QToolButton {"
            "  background:#2a2a2a; color:#cccccc; border:none;"
            "  border-bottom:1px solid #1a1a1a; text-align:left;"
            "  padding-left:6px; font-size:11px; font-weight:bold;"
            "}"
            "QToolButton:hover   { background:#333333; }"
            "QToolButton:checked { background:#2e3a4e; color:#a8c8f0; }"
        );

        // ── Container ─────────────────────────────────────────────────────────
        m_container = new QWidget(this);
        m_container->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        m_container->setMinimumHeight(0);

        auto *cLay = new QVBoxLayout(m_container);
        cLay->setContentsMargins(0, 2, 0, 4);
        cLay->setSpacing(0);
        cLay->addWidget(content);

        m_naturalH = naturalHeight();
        m_container->setMaximumHeight(startOpen ? m_naturalH : 0);

        // ── Separator ─────────────────────────────────────────────────────────
        auto *sep = new QFrame(this);
        sep->setFrameShape(QFrame::HLine);
        sep->setFixedHeight(1);
        sep->setStyleSheet("background:#1a1a1a;");

        lay->addWidget(m_btn);
        lay->addWidget(m_container);
        lay->addWidget(sep);

        // ── Animation ─────────────────────────────────────────────────────────
        m_anim = new QPropertyAnimation(this, "contentHeight", this);
        m_anim->setDuration(100);
        m_anim->setEasingCurve(QEasingCurve::InOutQuad);

        connect(m_btn, &QToolButton::toggled, this, &AccordionSection::toggle);

        connect(m_anim, &QPropertyAnimation::finished, this, [this]() {
            QWidget *w = parentWidget();
            while (w) {
                if (auto *dock = qobject_cast<QDockWidget *>(w)) {
                    dock->adjustSize();
                    break;
                }
                w = w->parentWidget();
            }
        });
    }

    int contentHeight() const { return m_container->maximumHeight(); }

    void setContentHeight(int h)
    {
        m_container->setMaximumHeight(h);
        updateGeometry();
    }

    void toggle(bool open)
    {
        m_btn->setArrowType(open ? Qt::DownArrow : Qt::RightArrow);
        if (open) {
            const int n = naturalHeight();
            if (n > 20) m_naturalH = n;
        }
        m_anim->stop();
        m_anim->setStartValue(m_container->maximumHeight());
        m_anim->setEndValue(open ? m_naturalH : 0);
        m_anim->start();
    }

private:
    int naturalHeight() const
    {
        int h = m_content->sizeHint().height();
        if (h < 20) h = m_content->minimumSizeHint().height();
        if (h < 20) h = m_content->minimumHeight();
        return h + 6;
    }

    QToolButton        *m_btn       = nullptr;
    QWidget            *m_container = nullptr;
    QWidget            *m_content   = nullptr;
    QPropertyAnimation *m_anim      = nullptr;
    int                 m_naturalH  = 180;
};

#include "ColorPanelWidget.moc"

// ─────────────────────────────────────────────────────────────────────────────
// ColorPanelWidget
// ─────────────────────────────────────────────────────────────────────────────
ColorPanelWidget::ColorPanelWidget(QWidget *parent)
    : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    setMinimumWidth(180);

    m_wheel    = new ColorWheelWidget(this);
    m_sliders  = new ColorSlidersWidget(this);
    m_swatches = new ColorSwatchWidget(this);
    m_scratch  = new ScratchPadWidget(this);

    m_wheel->setMinimumSize(180, 180);

    m_sliders->setMinimumHeight(140);
    // Prevent sliders from demanding more width than the dock provides
    m_sliders->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);

    // Swatches: Expanding width so it fills the dock and reflows columns,
    // Fixed height so the accordion knows exactly how tall it is.
    m_swatches->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    m_scratch->setMinimumHeight(140);
    m_scratch->setMaximumHeight(300);

    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);

    lay->addWidget(new AccordionSection(tr("  ◎  Color Wheel"), m_wheel,    true,  this));
    lay->addWidget(new AccordionSection(tr("  ≡  RGB / HSV"),   m_sliders,  false, this));
    auto *swatchScroll = new QScrollArea(this);
    swatchScroll->setWidget(m_swatches);
    swatchScroll->setWidgetResizable(true);
    swatchScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    swatchScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    swatchScroll->setFrameShape(QFrame::NoFrame);
    swatchScroll->setFixedHeight(5 * (22 + 2) + 2 * 4);  // 5 visible rows
    lay->addWidget(new AccordionSection(tr("  ▦  Swatches"), swatchScroll, false, this));
    lay->addWidget(new AccordionSection(tr("  ✏  Scratch Pad"), m_scratch,  false, this));
    lay->addStretch(1);

    connect(m_wheel,    &ColorWheelWidget::colorChanged,   this, [this](const QColor &c){ syncColor(c, m_wheel);    });
    connect(m_sliders,  &ColorSlidersWidget::colorChanged, this, [this](const QColor &c){ syncColor(c, m_sliders);  });
    connect(m_swatches, &ColorSwatchWidget::colorChanged,  this, [this](const QColor &c){ syncColor(c, m_swatches); });
    connect(m_scratch,  &ScratchPadWidget::colorPicked,    this, [this](const QColor &c){ syncColor(c, m_scratch);  });
}

QColor ColorPanelWidget::color() const { return m_wheel->color(); }

void ColorPanelWidget::setBrushEngine(BrushEngine *engine)
{
    m_scratch->setBrushEngine(engine);
}

void ColorPanelWidget::setColor(const QColor &c)
{
    QSignalBlocker b1(m_wheel), b2(m_sliders), b3(m_swatches);
    m_wheel->setColor(c);
    m_sliders->setColor(c);
    m_swatches->setColor(c);
    m_scratch->setColor(c);
}

void ColorPanelWidget::syncColor(const QColor &c, QObject *source)
{
    if (source != m_wheel)    { QSignalBlocker b(m_wheel);    m_wheel->setColor(c);    }
    if (source != m_sliders)  { QSignalBlocker b(m_sliders);  m_sliders->setColor(c);  }
    if (source != m_swatches) { QSignalBlocker b(m_swatches); m_swatches->setColor(c); }
    m_scratch->setColor(c);
    emit colorChanged(c);
}
