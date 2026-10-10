#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace mydesk::draft {

struct KeyBinding {
    std::string key;       // e.g. "L", "Ctrl+Z", "Escape"
    std::string commandId; // registry id
    std::string notes;
};

struct KeymapProfile {
    std::string id{"autocad-inspired-v1"};
    std::string label{"AutoCAD-inspired (MyDesk defaults)"};
    std::vector<KeyBinding> bindings;
};

[[nodiscard]] KeymapProfile defaultAutocadInspiredKeymap();
[[nodiscard]] std::unordered_map<std::string, std::string> bindingIndex(const KeymapProfile& profile);

} // namespace mydesk::draft
