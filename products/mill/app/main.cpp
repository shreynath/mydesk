// SPDX-License-Identifier: MPL-2.0
// MyDesk Mill — headless CLI entry point
#include "mydesk/mill/mill_document.hpp"
#include <cstdio>

int main() {
    mydesk::mill::MillDocument doc;
    std::printf("MyDesk Mill engine ready. Tools: %zu\n", doc.tool_count());
    return 0;
}
