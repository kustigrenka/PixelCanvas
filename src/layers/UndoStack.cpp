#include "UndoStack.h"

UndoStack::UndoStack(QObject *parent)
    : QObject(parent)
{}

void UndoStack::pushSnapshot(int layerIndex, const QImage &pixels)
{
    if (m_undoStack.size() >= kMaxSteps)
        m_undoStack.removeFirst();

    m_undoStack.push({ layerIndex, pixels.copy() });
    m_redoStack.clear();

    emit undoAvailable(true);
    emit redoAvailable(false);
}

// Save current state to redo stack, restore and return the undo snapshot.
Snapshot UndoStack::undo(int currentLayerIndex, const QImage &currentPixels)
{
    Q_ASSERT(canUndo());
    m_redoStack.push({ currentLayerIndex, currentPixels.copy() });
    Snapshot snap = m_undoStack.pop();
    emit undoAvailable(canUndo());
    emit redoAvailable(true);
    return snap;
}

// Save current state to undo stack, restore and return the redo snapshot.
Snapshot UndoStack::redo(int currentLayerIndex, const QImage &currentPixels)
{
    Q_ASSERT(canRedo());
    m_undoStack.push({ currentLayerIndex, currentPixels.copy() });
    Snapshot snap = m_redoStack.pop();
    emit undoAvailable(true);
    emit redoAvailable(canRedo());
    return snap;
}

void UndoStack::clear()
{
    m_undoStack.clear();
    m_redoStack.clear();
    emit undoAvailable(false);
    emit redoAvailable(false);
}
