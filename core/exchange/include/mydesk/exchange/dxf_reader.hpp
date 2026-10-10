#pragma once

#include "mydesk/document/document.hpp"
#include "mydesk/exchange/native_format.hpp"

#include <filesystem>

namespace mydesk::exchange {

// ASCII DXF import for LINE, CIRCLE, ARC, LWPOLYLINE, LAYER (partial).
// Status: import-only for supported entities — never claim full DXF/DWG fidelity.
[[nodiscard]] ConversionReport importDxf(document::Document& doc, const std::filesystem::path& path);

} // namespace mydesk::exchange
