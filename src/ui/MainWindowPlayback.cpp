#include "MainWindow.h"
#include "CanvasWidget.h"
#include "LayerStack.h"
#include "PlaybackWindow.h"

#include <QAction>
#include <QMessageBox>

// ─────────────────────────────────────────────────────────────────────────────
// Recording toggle
// ─────────────────────────────────────────────────────────────────────────────

void MainWindow::onToggleRecording()
{
    if (m_canvas->isRecording())
    {
        m_canvas->stopRecording();
        m_recordAction->setText(tr("⏺  Start Recording"));
        m_recordAction->setChecked(false);
        m_playAction->setEnabled(m_canvas->recorder().hasRecording());

        const int eventCount = m_canvas->recorder().events().size();
        if (eventCount > 0)
        {
            QMessageBox::information(this, tr("Recording Stopped"),
                tr("Recording stopped — %1 events captured.\n\n"
                   "Go to Playback → Play Recording to watch and export it.")
                .arg(eventCount));
        }
    }
    else
    {
        m_canvas->startRecording();
        m_recordAction->setText(tr("⏹  Stop Recording"));
        m_recordAction->setChecked(true);
        m_playAction->setEnabled(false);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Playback
// ─────────────────────────────────────────────────────────────────────────────

void MainWindow::onPlayback()
{
    if (!m_canvas->recorder().hasRecording())
    {
        QMessageBox::information(this, tr("No Recording"),
            tr("Record a drawing session first using Playback → Start Recording.\n\n"
               "Draw something while recording, then stop — you can then play it back "
               "and export as PNG frames or MP4 video."));
        return;
    }

    // Stop any in-progress recording before opening the playback window.
    if (m_canvas->isRecording())
    {
        m_canvas->stopRecording();
        m_recordAction->setText(tr("⏺  Start Recording"));
        m_recordAction->setChecked(false);
    }

    auto *win = new PlaybackWindow(
        m_canvas->recorder().events(),
        m_layerStack->canvasSize(),
        this);
    win->show();
    win->raise();
}
