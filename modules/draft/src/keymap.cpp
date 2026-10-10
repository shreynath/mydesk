#include "mydesk/draft/keymap.hpp"

namespace mydesk::draft {

KeymapProfile defaultAutocadInspiredKeymap() {
    KeymapProfile profile;
    profile.bindings = {
        {"L", "LINE", "Start LINE"},
        {"C", "CIRCLE", "Start CIRCLE"},
        {"E", "ERASE", "Erase selection"},
        {"U", "UNDO", "Undo"},
        {"Escape", "CANCEL", "Cancel active command"},
        {"Ctrl+Z", "UNDO", "Undo (OS common)"},
        {"Ctrl+Y", "REDO", "Redo (OS common)"},
        {"Meta+Z", "UNDO", "Undo on macOS"},
        {"Meta+Shift+Z", "REDO", "Redo on macOS"},
        {"Return", "REPEAT", "Repeat last command when idle"},
    };
    return profile;
}

std::unordered_map<std::string, std::string> bindingIndex(const KeymapProfile& profile) {
    std::unordered_map<std::string, std::string> index;
    for (const auto& binding : profile.bindings) {
        index[binding.key] = binding.commandId;
    }
    return index;
}

} // namespace mydesk::draft
