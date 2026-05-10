#include "MainWindow.h"
#include "CanvasWidget.h"
#include "LayerStack.h"
#include "PlaybackWindow.h"
#include <QAction>
#include <QMessageBox>

void MainWindow::onToggleRecording()
{
    if (m_canvas->isRecording()) {
        m_canvas->stopRecording();
        m_recordAction->setText(tr("⏺  Start Recording"));
        m_recordAction->setChecked(false);
        m_playAction->setEnabled(m_canvas->recorder().hasRecording());
    } else {
        m_canvas->startRecording();
        m_recordAction->setText(tr("⏹  Stop Recording"));
        m_recordAction->setChecked(true);
        m_playAction->setEnabled(false);
    }
}

void MainWindow::onPlayback()
{
    if (!m_canvas->recorder().hasRecording()) {
        QMessageBox::information(this, tr("No Recording"),
            tr("Record a drawing session first using Playback → Start Recording."));
        return;
    }

    if (m_canvas->isRecording()) {
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