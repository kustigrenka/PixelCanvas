#pragma once

#include <QObject>
#include <QStack>
#include <QImage>

struct Snapshot
{
    int    layerIndex = 0;
    QImage pixels;
};

class UndoStack : public QObject
{
    Q_OBJECT

public:
    static constexpr int kMaxSteps = 50;

    explicit UndoStack(QObject *parent = nullptr);

    void pushSnapshot(int layerIndex, const QImage &pixels);

    bool canUndo() const { return !m_undoStack.isEmpty(); }
    bool canRedo() const { return !m_redoStack.isEmpty(); }

    Snapshot undo(int currentLayerIndex, const QImage &currentPixels);
    Snapshot redo(int currentLayerIndex, const QImage &currentPixels);

    int peekUndoLayerIndex() const { return canUndo() ? m_undoStack.top().layerIndex : 0; }
    int peekRedoLayerIndex() const { return canRedo() ? m_redoStack.top().layerIndex : 0; }

    void clear();
    

signals:
    void undoAvailable(bool available);
    void redoAvailable(bool available);

private:
    QStack<Snapshot> m_undoStack;
    QStack<Snapshot> m_redoStack;
};

