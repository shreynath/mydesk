#include "mydesk/document/history.hpp"

#include "mydesk/document/document.hpp"

namespace mydesk::document {

namespace {

void applyForward(Document& doc, const HistoryOp& op) {
    switch (op.kind) {
    case HistoryOpKind::AddEntity:
        doc.applyAddEntity(op.after);
        break;
    case HistoryOpKind::RemoveEntity:
        doc.applyRemoveEntity(op.before.id);
        break;
    case HistoryOpKind::ReplaceEntity:
        doc.applyReplaceEntity(op.after);
        break;
    }
}

void applyInverse(Document& doc, const HistoryOp& op) {
    switch (op.kind) {
    case HistoryOpKind::AddEntity:
        doc.applyRemoveEntity(op.after.id);
        break;
    case HistoryOpKind::RemoveEntity:
        doc.applyAddEntity(op.before);
        break;
    case HistoryOpKind::ReplaceEntity:
        doc.applyReplaceEntity(op.before);
        break;
    }
}

} // namespace

void HistoryStack::push(Transaction tx) {
    if (index_ > 0) {
        undo_.resize(undo_.size() - index_);
        index_ = 0;
    }
    undo_.push_back(std::move(tx));
}

bool HistoryStack::undo(Document& doc) {
    if (!canUndo()) {
        return false;
    }
    const Transaction& tx = undo_[undo_.size() - 1 - index_];
    for (auto it = tx.ops.rbegin(); it != tx.ops.rend(); ++it) {
        applyInverse(doc, *it);
    }
    ++index_;
    return true;
}

bool HistoryStack::redo(Document& doc) {
    if (!canRedo()) {
        return false;
    }
    --index_;
    const Transaction& tx = undo_[undo_.size() - 1 - index_];
    for (const auto& op : tx.ops) {
        applyForward(doc, op);
    }
    return true;
}

void HistoryStack::clear() {
    undo_.clear();
    index_ = 0;
}

} // namespace mydesk::document
