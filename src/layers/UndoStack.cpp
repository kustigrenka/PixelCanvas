#include "UndoStack.h"

UndoStack::UndoStack(QObject *parent)
    : QObject(parent)
{}

void UndoStack::pushSnapshot(int layerIndex, const QImage &pixels)
{
    // Discard redo history ahead of cursor
    if (m_cursor < m_snapshots.size() - 1)
        m_snapshots.resize(m_cursor + 1);

    // Enforce cap — keep one extra slot for the "before" state
    if (m_snapshots.size() >= kMaxSteps)
        m_snapshots.removeFirst();

    m_snapshots.append({ layerIndex, pixels.copy() });
    m_cursor = m_snapshots.size() - 1;

    emit undoAvailable(canUndo());
    emit redoAvailable(false);
    emit historyChanged();
}

Snapshot UndoStack::undo()
{
    Q_ASSERT(canUndo());
    --m_cursor;
    emit undoAvailable(canUndo());
    emit redoAvailable(true);
    emit historyChanged();
    return m_snapshots[m_cursor];
}

Snapshot UndoStack::redo()
{
    Q_ASSERT(canRedo());
    ++m_cursor;
    emit undoAvailable(true);
    emit redoAvailable(canRedo());
    emit historyChanged();
    return m_snapshots[m_cursor];
}

void UndoStack::clear()
{
    m_snapshots.clear();
    m_cursor = -1;
    emit undoAvailable(false);
    emit redoAvailable(false);
    emit historyChanged();
}