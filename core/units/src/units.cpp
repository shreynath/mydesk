#include "mydesk/units/units.hpp"

#include <cmath>
#include <sstream>
#include <stdexcept>

namespace mydesk::units {

namespace {

constexpr double kMmPerInch = 25.4;

} // namespace

double toMillimeters(double value, LengthUnit unit) {
    switch (unit) {
    case LengthUnit::Millimeter:
        return value;
    case LengthUnit::Centimeter:
        return value * 10.0;
    case LengthUnit::Meter:
        return value * 1000.0;
    case LengthUnit::Inch:
        return value * kMmPerInch;
    case LengthUnit::Foot:
        return value * 12.0 * kMmPerInch;
    }
    throw std::invalid_argument("unknown length unit");
}

double fromMillimeters(double mm, LengthUnit unit) {
    switch (unit) {
    case LengthUnit::Millimeter:
        return mm;
    case LengthUnit::Centimeter:
        return mm / 10.0;
    case LengthUnit::Meter:
        return mm / 1000.0;
    case LengthUnit::Inch:
        return mm / kMmPerInch;
    case LengthUnit::Foot:
        return mm / (12.0 * kMmPerInch);
    }
    throw std::invalid_argument("unknown length unit");
}

std::string_view unitSymbol(LengthUnit unit) {
    switch (unit) {
    case LengthUnit::Millimeter:
        return "mm";
    case LengthUnit::Centimeter:
        return "cm";
    case LengthUnit::Meter:
        return "m";
    case LengthUnit::Inch:
        return "in";
    case LengthUnit::Foot:
        return "ft";
    }
    return "?";
}

std::string formatLength(double valueMm, const UnitSystem& system) {
    const double display = fromMillimeters(valueMm, system.length);
    std::ostringstream oss;
    oss.setf(std::ios::fixed);
    oss.precision(system.linearPrecision);
    oss << display << ' ' << unitSymbol(system.length);
    return oss.str();
}

} // namespace mydesk::units
