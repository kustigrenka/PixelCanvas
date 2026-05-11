#pragma once

#include <QObject>
#include <QVector>
#include <QImage>

// ─────────────────────────────────────────────────────────────────────────────
// Snapshot  –  a full copy of one layer's pixels at a point in time
// ─────────────────────────────────────────────────────────────────────────────

struct Snapshot
{
    int    layerIndex = 0;
    QImage pixels;
};

// ─────────────────────────────────────────────────────────────────────────────
// UndoStack
// ─────────────────────────────────────────────────────────────────────────────

class UndoStack : public QObject
{
    Q_OBJECT

public:
    static constexpr int kMaxSteps = 100;

    explicit UndoStack(QObject *parent = nullptr);

    void pushSnapshot(int layerIndex, const QImage &pixels);

    bool canUndo() const { return m_cursor > 0; }
    bool canRedo() const { return m_cursor < m_snapshots.size() - 1; }

    Snapshot undo();
    Snapshot redo();

    // ── History slider support ────────────────────────────────────────────────
    int      historySize()   const { return m_snapshots.size(); }
    int      currentIndex()  const { return m_cursor; }
    Snapshot snapshotAt(int index) const { return m_snapshots.value(index); }

    int peekUndoLayerIndex() const { return canUndo() ? m_snapshots[m_cursor - 1].layerIndex : 0; }
    int peekRedoLayerIndex() const { return canRedo() ? m_snapshots[m_cursor + 1].layerIndex : 0; }

    void clear();

signals:
    void undoAvailable(bool available);
    void redoAvailable(bool available);
    void historyChanged();   // emitted on push / undo / redo / clear

private:
    QVector<Snapshot> m_snapshots;
    int               m_cursor = -1;   // points to current state; -1 = empty
};
