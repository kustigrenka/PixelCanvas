// ─────────────────────────────────────────────────────────────────────────────
// PlaybackWindow.cpp
// ─────────────────────────────────────────────────────────────────────────────
#include "PlaybackWindow.h"
#include "BrushEngine.h"
#include "LayerStack.h"
#include "UndoStack.h"
#include "Layer.h"

#include <QPainter>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTimer>
#include <QFileDialog>
#include <QMessageBox>
#include <QDir>
#include <QProcess>
#include <QStandardPaths>
#include <QProgressDialog>
#include <QApplication>
#include <QDateTime>

// ─────────────────────────────────────────────────────────────────────────────
// PlaybackCanvas
// ─────────────────────────────────────────────────────────────────────────────
PlaybackCanvas::PlaybackCanvas(QWidget *parent) : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMinimumSize(400, 300);
}

void PlaybackCanvas::setImage(const QImage &img) { m_image = img; update(); }

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
    setMinimumSize(560, 500);
    setAttribute(Qt::WA_DeleteOnClose);

    // Isolated layer stack + brush engine
    m_undoStack   = new UndoStack(this);
    m_layerStack  = new LayerStack(this);
    m_brushEngine = new BrushEngine(this);
    m_layerStack->init(canvasSize);
    if (Layer *l = m_layerStack->activeLayer())
        m_brushEngine->setActiveLayer(&l->pixels);

    // ── Layout ────────────────────────────────────────────────────────────────
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(4);

    m_playCanvas = new PlaybackCanvas(this);
    layout->addWidget(m_playCanvas, 1);

    m_statusLabel = new QLabel(tr("Ready — press Play to watch the speedpaint"), this);
    m_statusLabel->setAlignment(Qt::AlignCenter);
    m_statusLabel->setStyleSheet("color:#aaa; font-size:11px;");
    layout->addWidget(m_statusLabel);

    m_progress = new QProgressBar(this);
    m_progress->setRange(0, m_events.size());
    m_progress->setValue(0);
    m_progress->setTextVisible(false);
    m_progress->setFixedHeight(6);
    m_progress->setStyleSheet(
        "QProgressBar { background:#2a2a2a; border:none; border-radius:3px; }"
        "QProgressBar::chunk { background:#4a90e2; border-radius:3px; }");
    layout->addWidget(m_progress);

    // ── Button row ────────────────────────────────────────────────────────────
    auto *btnRow = new QHBoxLayout;
    btnRow->setSpacing(6);

    m_playBtn = new QPushButton(tr("▶  Play"), this);
    m_playBtn->setFixedHeight(30);
    m_playBtn->setStyleSheet(
        "QPushButton { background:#2e5c99; color:#fff; border:none; border-radius:4px; "
        "font-weight:bold; padding:0 12px; }"
        "QPushButton:hover { background:#3a74c2; }"
        "QPushButton:disabled { background:#333; color:#666; }");

    m_stopBtn = new QPushButton(tr("■  Stop"), this);
    m_stopBtn->setEnabled(false);
    m_stopBtn->setFixedHeight(30);
    m_stopBtn->setStyleSheet(
        "QPushButton { background:#7a2a2a; color:#fff; border:none; border-radius:4px; "
        "font-weight:bold; padding:0 12px; }"
        "QPushButton:hover { background:#c0392b; }"
        "QPushButton:disabled { background:#333; color:#666; }");

    m_saveBtn = new QPushButton(tr("💾  Save Video…"), this);
    m_saveBtn->setEnabled(false);
    m_saveBtn->setFixedHeight(30);
    m_saveBtn->setToolTip(tr("Save the speedpaint as a PNG image sequence or MP4 video"));
    m_saveBtn->setStyleSheet(
        "QPushButton { background:#2a6a3a; color:#fff; border:none; border-radius:4px; "
        "font-weight:bold; padding:0 12px; }"
        "QPushButton:hover { background:#27ae60; }"
        "QPushButton:disabled { background:#333; color:#666; }");

    auto *speedLabel = new QLabel(tr("Speed:"), this);
    speedLabel->setStyleSheet("color:#aaa; font-size:11px;");
    m_speedCombo = new QComboBox(this);
    m_speedCombo->addItem(tr("1×"),   1.0);
    m_speedCombo->addItem(tr("2×"),   2.0);
    m_speedCombo->addItem(tr("4×"),   4.0);
    m_speedCombo->addItem(tr("8×"),   8.0);
    m_speedCombo->addItem(tr("16×"), 16.0);
    m_speedCombo->setCurrentIndex(2);
    m_speed = 4.0;
    m_speedCombo->setFixedHeight(30);

    connect(m_speedCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this]() { m_speed = m_speedCombo->currentData().toDouble(); });

    btnRow->addWidget(m_playBtn);
    btnRow->addWidget(m_stopBtn);
    btnRow->addWidget(m_saveBtn);
    btnRow->addStretch();
    btnRow->addWidget(speedLabel);
    btnRow->addWidget(m_speedCombo);
    layout->addLayout(btnRow);

    // ── Info label ────────────────────────────────────────────────────────────
    auto *info = new QLabel(
        tr("💡 After playback finishes, click Save Video to export as PNG frames or MP4"), this);
    info->setStyleSheet("color:#555; font-size:10px;");
    info->setAlignment(Qt::AlignCenter);
    layout->addWidget(info);

    connect(m_playBtn, &QPushButton::clicked, this, &PlaybackWindow::startPlayback);
    connect(m_stopBtn, &QPushButton::clicked, this, &PlaybackWindow::stopPlayback);
    connect(m_saveBtn, &QPushButton::clicked, this, &PlaybackWindow::saveRecording);

    recomposite();
}

PlaybackWindow::~PlaybackWindow() = default;

// ─────────────────────────────────────────────────────────────────────────────
void PlaybackWindow::recomposite()
{
    QImage composited(m_canvasSize, QImage::Format_ARGB32_Premultiplied);
    composited.fill(Qt::white);
    m_layerStack->recompositeRect(composited, composited.rect());
    m_playCanvas->setImage(composited);

    // Capture frame if saving
    if (m_capturing)
        m_frames.append(composited.copy());
}

// ─────────────────────────────────────────────────────────────────────────────
void PlaybackWindow::startPlayback()
{
    if (m_events.isEmpty()) return;

    m_stopped   = false;
    m_capturing = true;
    m_frames.clear();

    // Reset canvas
    m_layerStack->init(m_canvasSize);
    if (Layer *l = m_layerStack->activeLayer())
        m_brushEngine->setActiveLayer(&l->pixels);
    recomposite();

    m_index = 0;
    m_progress->setValue(0);
    m_playBtn->setEnabled(false);
    m_stopBtn->setEnabled(true);
    m_saveBtn->setEnabled(false);
    m_statusLabel->setText(tr("Recording playback frames…"));
    scheduleNext();
}

// ─────────────────────────────────────────────────────────────────────────────
void PlaybackWindow::stopPlayback()
{
    m_stopped   = true;   // guards all pending QTimer::singleShot callbacks
    m_capturing = false;
    m_brushEngine->endStroke();
    recomposite();
    finishPlayback(false);
}

// ─────────────────────────────────────────────────────────────────────────────
void PlaybackWindow::finishPlayback(bool completed)
{
    m_playBtn->setEnabled(true);
    m_stopBtn->setEnabled(false);
    m_saveBtn->setEnabled(!m_frames.isEmpty());
    m_statusLabel->setText(completed
        ? tr("Playback complete — %1 frames captured. Click Save Video to export.")
          .arg(m_frames.size())
        : tr("Stopped — %1 frames captured. Click Save Video to export.")
          .arg(m_frames.size()));
    emit playbackFinished();
}

// ─────────────────────────────────────────────────────────────────────────────
void PlaybackWindow::fireEvent()
{
    // Stop guard — prevents late timer fires after stopPlayback()
    if (m_stopped) return;

    if (m_index >= m_events.size()) {
        m_brushEngine->endStroke();
        m_capturing = false;
        recomposite();
        m_progress->setValue(m_events.size());
        finishPlayback(true);
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
        m_brushEngine->endStroke();
        recomposite();
    } else {
        m_brushEngine->addSample(ev.sample);
        if (m_index % 4 == 0)
            recomposite();
    }

    ++m_index;
    scheduleNext();
}

// ─────────────────────────────────────────────────────────────────────────────
void PlaybackWindow::scheduleNext()
{
    if (m_stopped) return;

    if (m_index >= m_events.size()) {
        QTimer::singleShot(0, this, &PlaybackWindow::fireEvent);
        return;
    }
    // Fix: safe delay calculation
    qint64 prev = (m_index > 0) ? m_events[m_index - 1].timestampMs : 0;
    qint64 next = m_events[m_index].timestampMs;
    int delay   = static_cast<int>(qMax(0LL, next - prev) / m_speed);
    delay       = qMin(delay, 500);   // cap at 500ms to avoid hanging on long pauses
    QTimer::singleShot(delay, this, &PlaybackWindow::fireEvent);
}

// ─────────────────────────────────────────────────────────────────────────────
// Save Recording
// ─────────────────────────────────────────────────────────────────────────────
void PlaybackWindow::saveRecording()
{
    if (m_frames.isEmpty()) {
        QMessageBox::warning(this, tr("No Frames"),
            tr("Play back the recording first to capture frames."));
        return;
    }

    // Ask what format to save
    QMessageBox fmt(this);
    fmt.setWindowTitle(tr("Save Speedpaint"));
    fmt.setText(tr("How would you like to save the speedpaint?"));
    auto *pngBtn = fmt.addButton(tr("PNG Frames"), QMessageBox::AcceptRole);
    auto *mp4Btn = fmt.addButton(tr("MP4 Video"), QMessageBox::AcceptRole);
    fmt.addButton(QMessageBox::Cancel);
    fmt.exec();

    if (fmt.clickedButton() == pngBtn) {
        // ── Save PNG sequence ─────────────────────────────────────────────────
        const QString dir = QFileDialog::getExistingDirectory(this,
            tr("Choose folder to save PNG frames"),
            QStandardPaths::writableLocation(QStandardPaths::PicturesLocation));
        if (dir.isEmpty()) return;

        const QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
        const QString folder    = dir + "/speedpaint_" + timestamp;
        QDir().mkpath(folder);

        QProgressDialog prog(tr("Saving frames…"), tr("Cancel"), 0, m_frames.size(), this);
        prog.setWindowModality(Qt::WindowModal);

        for (int i = 0; i < m_frames.size(); ++i) {
            prog.setValue(i);
            QApplication::processEvents();
            if (prog.wasCanceled()) break;
            const QString path = folder + QString("/frame_%1.png").arg(i, 5, 10, QChar('0'));
            m_frames[i].save(path, "PNG");
        }
        prog.setValue(m_frames.size());

        QMessageBox::information(this, tr("Saved"),
            tr("Saved %1 frames to:\n%2").arg(m_frames.size()).arg(folder));

    } else if (fmt.clickedButton() == mp4Btn) {
        // ── Save MP4 via ffmpeg ───────────────────────────────────────────────
        // Check ffmpeg is available
        QProcess check;
        check.start("ffmpeg", {"-version"});
        check.waitForFinished(3000);
        if (check.exitCode() != 0) {
            QMessageBox::warning(this, tr("ffmpeg not found"),
                tr("ffmpeg is required to export MP4.\n\n"
                   "Install it with:\n  sudo apt install ffmpeg\n\n"
                   "Or use PNG sequence export instead."));
            return;
        }

        const QString savePath = QFileDialog::getSaveFileName(this,
            tr("Save speedpaint as MP4"),
            QStandardPaths::writableLocation(QStandardPaths::MoviesLocation)
                + "/speedpaint_" + QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss") + ".mp4",
            tr("MP4 Video (*.mp4)"));
        if (savePath.isEmpty()) return;

        // Write frames to a temp folder
        const QString tmpDir = QDir::tempPath() + "/pixelcanvas_export";
        QDir().mkpath(tmpDir);

        QProgressDialog prog(tr("Preparing frames…"), tr("Cancel"), 0, m_frames.size(), this);
        prog.setWindowModality(Qt::WindowModal);

        for (int i = 0; i < m_frames.size(); ++i) {
            prog.setValue(i);
            QApplication::processEvents();
            if (prog.wasCanceled()) { QDir(tmpDir).removeRecursively(); return; }
            m_frames[i].save(tmpDir + QString("/frame_%1.png").arg(i, 5, 10, QChar('0')), "PNG");
        }

        prog.setLabelText(tr("Encoding MP4…"));
        prog.setMaximum(0);   // indeterminate while ffmpeg runs
        QApplication::processEvents();

        // Run ffmpeg: 24fps, high quality
        QProcess ffmpeg;
        ffmpeg.start("ffmpeg", {
            "-y",
            "-framerate", "24",
            "-i", tmpDir + "/frame_%05d.png",
            "-c:v", "libx264",
            "-pix_fmt", "yuv420p",
            "-crf", "18",
            savePath
        });
        ffmpeg.waitForFinished(300000);   // wait up to 5 minutes

        // Clean up temp frames
        QDir(tmpDir).removeRecursively();

        prog.reset();

        if (ffmpeg.exitCode() == 0) {
            QMessageBox::information(this, tr("Saved"),
                tr("Speedpaint saved to:\n%1").arg(savePath));
        } else {
            QMessageBox::warning(this, tr("ffmpeg error"),
                tr("ffmpeg failed:\n%1").arg(
                    QString::fromUtf8(ffmpeg.readAllStandardError())));
        }
    }
}