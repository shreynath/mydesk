#pragma once

#include "mydesk/document/document.hpp"

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace mydesk::exchange {

struct ConversionReport {
    bool ok{false};
    std::string format{"mydesk-native"};
    std::string formatVersion;
    std::size_t entityCount{0};
    std::size_t layerCount{0};
    std::vector<std::string> warnings;
    std::vector<std::string> errors;
};

// Native editable format: JSON document with schemaVersion.
// Extension: .mydesk
[[nodiscard]] ConversionReport saveNative(const document::Document& doc,
                                          const std::filesystem::path& path);
[[nodiscard]] ConversionReport loadNative(document::Document& doc,
                                          const std::filesystem::path& path);

[[nodiscard]] std::string serializeNative(const document::Document& doc);
[[nodiscard]] ConversionReport deserializeNative(document::Document& doc, std::string_view json);

} // namespace mydesk::exchange
