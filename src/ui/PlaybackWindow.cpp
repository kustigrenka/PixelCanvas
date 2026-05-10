#include "PlaybackWindow.h"
#include "BrushEngine.h"
#include "LayerStack.h"
#include "UndoStack.h"
#include "Layer.h"

#include <QPainter>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTimer>

// ─────────────────────────────────────────────────────────────────────────────
// PlaybackCanvas
// ─────────────────────────────────────────────────────────────────────────────

PlaybackCanvas::PlaybackCanvas(QWidget *parent)
    : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMinimumSize(400, 300);
}

void PlaybackCanvas::setImage(const QImage &img)
{
    m_image = img;
    update();
}

void PlaybackCanvas::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), QColor(0x6E, 0x6E, 0x6E));
    if (m_image.isNull()) return;

    const QSizeF scaled = m_image.size().scaled(size(), Qt::KeepAspectRatio);
    const QRectF dst(
        (width()  - scaled.width())  * 0.5,
        (height() - scaled.height()) * 0.5,
        scaled.width(), scaled.height());

    p.setRenderHint(QPainter::SmoothPixmapTransform);
    p.fillRect(dst, Qt::white);
    p.drawImage(dst, m_image);
    p.setPen(QPen(QColor(0x30, 0x30, 0x30), 1));
    p.setBrush(Qt::NoBrush);
    p.drawRect(dst.adjusted(0, 0, -1, -1));
}

// ─────────────────────────────────────────────────────────────────────────────
// PlaybackWindow
// ─────────────────────────────────────────────────────────────────────────────

PlaybackWindow::PlaybackWindow(const QVector<RecordedEvent> &events,
                               const QSize &canvasSize,
                               QWidget *parent)
    : QWidget(parent, Qt::Window)
    , m_events(events)
    , m_canvasSize(canvasSize)
{
    setWindowTitle(tr("Speedpaint Playback"));
    setMinimumSize(500, 450);
    setAttribute(Qt::WA_DeleteOnClose);

    // Isolated layer stack + brush engine
    m_undoStack   = new UndoStack(this);
    m_layerStack  = new LayerStack(this);
    m_brushEngine = new BrushEngine(this);

    m_layerStack->init(canvasSize);
    if (Layer *l = m_layerStack->activeLayer())
        m_brushEngine->setActiveLayer(&l->pixels);

    // Layout
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    m_playCanvas = new PlaybackCanvas(this);
    layout->addWidget(m_playCanvas, 1);

    m_statusLabel = new QLabel(tr("Ready — press Play to watch"), this);
    m_statusLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_statusLabel);

    m_progress = new QProgressBar(this);
    m_progress->setRange(0, m_events.size());
    m_progress->setValue(0);
    layout->addWidget(m_progress);

    auto *btnRow = new QHBoxLayout;

    m_playBtn = new QPushButton(tr("▶ Play"), this);
    m_stopBtn = new QPushButton(tr("■ Stop"), this);
    m_stopBtn->setEnabled(false);

    auto *speedLabel = new QLabel(tr("Speed:"), this);
    m_speedCombo = new QComboBox(this);
    m_speedCombo->addItem(tr("1×"),   1.0);
    m_speedCombo->addItem(tr("2×"),   2.0);
    m_speedCombo->addItem(tr("4×"),   4.0);
    m_speedCombo->addItem(tr("8×"),   8.0);
    m_speedCombo->addItem(tr("16×"), 16.0);
    m_speedCombo->setCurrentIndex(2);
    m_speed = 4.0;

    connect(m_speedCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this]() {
        m_speed = m_speedCombo->currentData().toDouble();
    });

    btnRow->addWidget(m_playBtn);
    btnRow->addWidget(m_stopBtn);
    btnRow->addStretch();
    btnRow->addWidget(speedLabel);
    btnRow->addWidget(m_speedCombo);
    layout->addLayout(btnRow);

    connect(m_playBtn, &QPushButton::clicked, this, &PlaybackWindow::startPlayback);
    connect(m_stopBtn, &QPushButton::clicked, this, &PlaybackWindow::stopPlayback);

    // Show blank white canvas initially
    recomposite();
}

PlaybackWindow::~PlaybackWindow() = default;

void PlaybackWindow::recomposite()
{
    QImage composited(m_canvasSize, QImage::Format_ARGB32_Premultiplied);
    composited.fill(Qt::white);
    m_layerStack->recompositeRect(composited, composited.rect());
    m_playCanvas->setImage(composited);
}

void PlaybackWindow::startPlayback()
{
    if (m_events.isEmpty()) return;

    // Reset to blank
    m_layerStack->init(m_canvasSize);
    if (Layer *l = m_layerStack->activeLayer())
        m_brushEngine->setActiveLayer(&l->pixels);
    recomposite();

    m_index = 0;
    m_progress->setValue(0);
    m_playBtn->setEnabled(false);
    m_stopBtn->setEnabled(true);
    m_statusLabel->setText(tr("Playing..."));
    scheduleNext();
}

void PlaybackWindow::stopPlayback()
{
    m_brushEngine->endStroke();
    recomposite();
    m_playBtn->setEnabled(true);
    m_stopBtn->setEnabled(false);
    m_statusLabel->setText(tr("Stopped"));
    emit playbackFinished();
}

void PlaybackWindow::fireEvent()
{
    if (m_index >= m_events.size()) {
        m_brushEngine->endStroke();
        recomposite();
        m_playBtn->setEnabled(true);
        m_stopBtn->setEnabled(false);
        m_statusLabel->setText(tr("Done"));
        m_progress->setValue(m_events.size());
        emit playbackFinished();
        return;
    }

    const RecordedEvent &ev = m_events[m_index];
    m_progress->setValue(m_index);

    if (ev.isBegin) {
        if (Layer *l = m_layerStack->activeLayer())
            m_brushEngine->setActiveLayer(&l->pixels);
        m_brushEngine->setSettings(ev.settings);
        m_brushEngine->setColor(ev.color);
        m_brushEngine->beginStroke();
    } else if (ev.isEnd) {
        const QRect dirty = m_brushEngine->endStroke();
        Q_UNUSED(dirty)
        recomposite();
    } else {
        m_brushEngine->addSample(ev.sample);
        if (m_index % 8 == 0)
            recomposite();
    }

    ++m_index;
    scheduleNext();
}

void PlaybackWindow::scheduleNext()
{
    if (m_index >= m_events.size()) {
        QTimer::singleShot(0, this, &PlaybackWindow::fireEvent);
        return;
    }
    const qint64 prev  = m_events[m_index - (m_index > 0 ? 1 : 0)].timestampMs;
    const qint64 next  = m_events[m_index].timestampMs;
    const int    delay = static_cast<int>(qMax(0LL, next - prev) / m_speed);
    QTimer::singleShot(delay, this, &PlaybackWindow::fireEvent);
}