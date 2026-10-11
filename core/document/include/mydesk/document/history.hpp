#pragma once

#include "mydesk/document/entity.hpp"

#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace mydesk::document {

class Document;

enum class HistoryOpKind {
    AddEntity,
    RemoveEntity,
    ReplaceEntity,
    ReplaceLayer,
    SetActiveLayer,
};

struct HistoryOp {
    HistoryOp() = default;
    HistoryOp(HistoryOpKind opKind, Entity beforeEntity, Entity afterEntity)
        : kind(opKind), before(std::move(beforeEntity)), after(std::move(afterEntity)) {}

    HistoryOpKind kind{HistoryOpKind::AddEntity};
    Entity before;
    Entity after;
    Layer beforeLayer;
    Layer afterLayer;
    LayerId beforeActiveLayer{kInvalidLayerId};
    LayerId afterActiveLayer{kInvalidLayerId};
};

struct Transaction {
    std::string label;
    std::vector<HistoryOp> ops;
};

class HistoryStack {
public:
    void push(Transaction tx);
    [[nodiscard]] bool canUndo() const { return undo_.size() > index_; }
    [[nodiscard]] bool canRedo() const { return index_ > 0; }
    [[nodiscard]] std::size_t undoDepth() const { return undo_.size() - index_; }
    [[nodiscard]] std::size_t redoDepth() const { return index_; }

    // Applies inverse of next undoable transaction onto document.
    bool undo(Document& doc);
    bool redo(Document& doc);
    void clear();

private:
    // Newest transaction is at the back of undo_ when index_ == 0.
    // index_ counts how many transactions have been undone (also the redo cursor).
    std::vector<Transaction> undo_;
    std::size_t index_{0};
};

} // namespace mydesk::document
