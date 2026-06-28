#pragma once

#include <deque>
#include "../Model/Scene.h"

namespace Worldizer
{
/**
    Snapshot-based linear undo for the geometry editor. Each entry is a complete
    `Scene` copy (cheap — at most a few sectors + a couple of mics + a small brush
    list per snapshot, and the stack is capped at 20 entries). Snapshot-based avoids
    the complexity of typed inverse operations and is bulletproof: undoing always
    restores exactly the state at push time. No redo for Slice 6a.
*/
class UndoStack
{
public:
    static constexpr size_t kMaxDepth = 20;

    /** Push a "before this edit" snapshot. The oldest entry is dropped if the stack
        is at capacity. */
    void push (const Scene& sceneBeforeEdit)
    {
        snapshots.push_back (sceneBeforeEdit);
        while (snapshots.size() > kMaxDepth)
            snapshots.pop_front();
    }

    bool canUndo() const noexcept { return ! snapshots.empty(); }

    /** Pop and return the most recent pre-edit snapshot. Caller applies it. */
    Scene undo()
    {
        Scene s = snapshots.back();
        snapshots.pop_back();
        return s;
    }

    void clear() noexcept { snapshots.clear(); }
    size_t getDepth() const noexcept { return snapshots.size(); }

private:
    std::deque<Scene> snapshots;
};
} // namespace Worldizer
