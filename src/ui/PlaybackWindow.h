#pragma once
#include <QWidget>
#include <QProgressBar>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>
#include <QImage>
#include <QTimer>
#include <QVector>
#include "StrokeRecorder.h"

class BrushEngine;
class LayerStack;
class UndoStack;

class PlaybackCanvas : public QWidget
{
    Q_OBJECT
public:
    explicit PlaybackCanvas(QWidget *parent = nullptr);
    void setImage(const QImage &img);
    const QImage &image() const { return m_image; }

protected:
    void paintEvent(QPaintEvent *) override;

private:
    QImage m_image;
};

class PlaybackWindow : public QWidget
{
    Q_OBJECT
public:
    explicit PlaybackWindow(const QVector<RecordedEvent> &events,
                            const QSize &canvasSize,
                            QWidget *parent = nullptr);
    ~PlaybackWindow() override;

signals:
    void playbackFinished();

private slots:
    void startPlayback();
    void stopPlayback();
    void fireEvent();
    void saveRecording();

private:
    void scheduleNext();
    void recomposite();
    void finishPlayback(bool completed);

    QVector<RecordedEvent> m_events;
    QSize                  m_canvasSize;
    int                    m_index   = 0;
    double                 m_speed   = 4.0;
    bool                   m_stopped = false;   // guards against late timer fires

    // Isolated engine — never touches the main canvas
    LayerStack  *m_layerStack  = nullptr;
    BrushEngine *m_brushEngine = nullptr;
    UndoStack   *m_undoStack   = nullptr;

    PlaybackCanvas *m_playCanvas   = nullptr;
    QLabel         *m_statusLabel  = nullptr;
    QProgressBar   *m_progress     = nullptr;
    QPushButton    *m_playBtn      = nullptr;
    QPushButton    *m_stopBtn      = nullptr;
    QPushButton    *m_saveBtn      = nullptr;
    QComboBox      *m_speedCombo   = nullptr;

    // Accumulated frames for saving
    QVector<QImage> m_frames;
    bool            m_capturing = false;
};