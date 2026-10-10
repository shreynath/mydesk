// SPDX-License-Identifier: MPL-2.0
// MyDesk Animate — headless CLI entry point
#include "mydesk/animate/animate_document.hpp"
#include <cstdio>

int main() {
    mydesk::animate::AnimateDocument doc;
    std::printf("MyDesk Animate engine ready. Nodes: %zu\n", doc.node_count());
    return 0;
}
