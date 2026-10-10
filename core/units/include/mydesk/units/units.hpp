#pragma once

#include <string>
#include <string_view>

namespace mydesk::units {

enum class LengthUnit {
    Millimeter,
    Centimeter,
    Meter,
    Inch,
    Foot,
};

struct UnitSystem {
    LengthUnit length{LengthUnit::Millimeter};
    int linearPrecision{4};
    double modelSpaceTolerance{1e-6};
};

[[nodiscard]] double toMillimeters(double value, LengthUnit unit);
[[nodiscard]] double fromMillimeters(double mm, LengthUnit unit);
[[nodiscard]] std::string_view unitSymbol(LengthUnit unit);
[[nodiscard]] std::string formatLength(double valueMm, const UnitSystem& system);

} // namespace mydesk::units
