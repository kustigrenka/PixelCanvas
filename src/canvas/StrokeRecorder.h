#pragma once

#include <QVector>
#include <QElapsedTimer>
#include <QColor>

#include "Stroke.h"
#include "BrushSettings.h"

// ─────────────────────────────────────────────────────────────────────────────
// RecordedEvent  –  a single timestamped entry in a recording session
// ─────────────────────────────────────────────────────────────────────────────

struct RecordedEvent
{
    qint64        timestampMs = 0;
    StrokeSample  sample;
    bool          isBegin    = false;
    bool          isEnd      = false;
    BrushSettings settings;
    QColor        color;
    int           layerIndex = 0;
};

// ─────────────────────────────────────────────────────────────────────────────
// StrokeRecorder  –  records all stroke events for later playback
// ─────────────────────────────────────────────────────────────────────────────

class StrokeRecorder
{
public:
    void startRecording()
    {
        m_events.clear();
        m_timer.start();
        m_recording = true;
    }

    void stopRecording()
    {
        m_recording = false;
    }

    bool isRecording() const { return m_recording; }

    void recordBegin(const BrushSettings &s, const QColor &c, int layerIdx)
    {
        if (!m_recording) return;
        RecordedEvent e;
        e.timestampMs = m_timer.elapsed();
        e.isBegin     = true;
        e.settings    = s;
        e.color       = c;
        e.layerIndex  = layerIdx;
        m_events.append(e);
    }

    void recordSample(const StrokeSample &s)
    {
        if (!m_recording) return;
        RecordedEvent e;
        e.timestampMs = m_timer.elapsed();
        e.sample      = s;
        m_events.append(e);
    }

    void recordEnd()
    {
        if (!m_recording) return;
        RecordedEvent e;
        e.timestampMs = m_timer.elapsed();
        e.isEnd       = true;
        m_events.append(e);
    }

    const QVector<RecordedEvent> &events()       const { return m_events; }
    bool                          hasRecording() const { return !m_events.isEmpty(); }

private:
    QVector<RecordedEvent> m_events;
    QElapsedTimer          m_timer;
    bool                   m_recording = false;
};
