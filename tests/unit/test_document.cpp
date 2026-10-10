#include "../test_assert.hpp"

#include "mydesk/document/document.hpp"

void test_document_line_undo_redo() {
    mydesk::document::Document doc;
    expect(doc.entities().empty(), "starts empty");
    const auto id = doc.addLine({0, 0}, {10, 0});
    expect(doc.entities().size() == 1, "one entity");
    expect(doc.findEntity(id) != nullptr, "find line");
    expect(doc.canUndo(), "can undo");
    expect(doc.undo(), "undo ok");
    expect(doc.entities().empty(), "empty after undo");
    expect(doc.redo(), "redo ok");
    expect(doc.entities().size() == 1, "restored");
    expect(doc.findEntity(id) != nullptr, "same id after redo");
}

void test_circle_reject_nonpositive() {
    mydesk::document::Document doc;
    bool threw = false;
    try {
        doc.addCircle({0, 0}, 0.0);
    } catch (...) {
        threw = true;
    }
    expect(threw, "zero radius rejected");
    expect(doc.entities().empty(), "no entity on failure");
}
