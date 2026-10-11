// SPDX-License-Identifier: MPL-2.0
#include "main_window.hpp"

#include <QApplication>
#include <QComboBox>
#include <QLineEdit>
#include <QTreeWidget>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

namespace {
int failures = 0;
#define CHECK(condition) do { if (!(condition)) { \
    std::fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition); ++failures; \
} } while (false)
}

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    mydesk::draft_lt::MainWindow window;
    window.show();
    app.processEvents();

    CHECK(window.findChild<QTreeWidget*>(QStringLiteral("draftLtLayerTree")) != nullptr);
    CHECK(window.findChild<QLineEdit*>(QStringLiteral("commandInput")) != nullptr);
    CHECK(window.findChild<QWidget*>(QStringLiteral("draftLtViewport")) != nullptr);

    CHECK(window.executeCommand(QStringLiteral("LAYER NEW WALLS")));
    CHECK(window.executeCommand(QStringLiteral("LAYER MAKE WALLS")));
    CHECK(window.executeCommand(QStringLiteral("LAYER LTYPE WALLS DASHED")));
    CHECK(window.executeCommand(QStringLiteral("LINE 0,0 120,0")));
    CHECK(window.executeCommand(QStringLiteral("CIRCLE 40,30 12")));
    CHECK(window.document().entities().size() == 2);

    CHECK(window.executeCommand(QStringLiteral("UNDO")));
    CHECK(window.document().entities().size() == 1);
    CHECK(window.executeCommand(QStringLiteral("REDO")));
    CHECK(window.document().entities().size() == 2);

    const auto firstId = window.document().entities().front().id;
    window.document().select(firstId);
    window.applySelectionProperties(QStringLiteral("WALLS"), QStringLiteral("CENTER"), 0.50,
                                    false);
    const auto* edited = window.document().findEntity(firstId);
    CHECK(edited != nullptr);
    CHECK(edited && edited->linetype == mydesk::document::Linetype::Center);
    CHECK(edited && !edited->lineweightByLayer);
    CHECK(edited && edited->lineweightMm == 0.50);

    const auto dir = std::filesystem::temp_directory_path() / "mydesk_draft_lt_ui";
    std::filesystem::create_directories(dir);
    const auto nativePath = dir / "roundtrip.mydesk";
    const auto dxfPath = dir / "roundtrip.dxf";
    CHECK(window.saveDocument(nativePath));
    CHECK(window.exportDxf(dxfPath));
    CHECK(std::filesystem::exists(nativePath));
    CHECK(std::filesystem::exists(dxfPath));

    mydesk::draft_lt::MainWindow reopened;
    CHECK(reopened.openDocument(nativePath));
    CHECK(reopened.document().entities().size() == 2);
    CHECK(reopened.document().findLayerByName("WALLS") != nullptr);
    std::ifstream dxf(dxfPath);
    const std::string dxfText((std::istreambuf_iterator<char>(dxf)),
                              std::istreambuf_iterator<char>());
    CHECK(dxfText.find("SECTION") != std::string::npos);
    CHECK(dxfText.find("LINE") != std::string::npos);

    if (failures == 0) std::printf("Draft LT Qt workspace tests passed.\n");
    return failures == 0 ? 0 : 1;
}
