#pragma once

#include "mydesk/document/document.hpp"
#include "mydesk/exchange/native_format.hpp"

#include <filesystem>

namespace mydesk::exchange {

// Minimal DXF R12-style ASCII export for LINE and CIRCLE only.
// Status: import-only not implemented; export is partial (entities + layers).
[[nodiscard]] ConversionReport exportDxfR12(const document::Document& doc,
                                            const std::filesystem::path& path);

} // namespace mydesk::exchange
