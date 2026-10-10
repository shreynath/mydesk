#pragma once

namespace mydesk::commands {

// AutoCAD-inspired drawing aids (F3/F7/F8/F9 style toggles in the UI).
struct DraftModes {
    bool objectSnap{true}; // F3
    bool gridDisplay{true};
    bool gridSnap{false}; // F9
    bool ortho{false};    // F8
    double gridSpacing{10.0};
};

} // namespace mydesk::commands
