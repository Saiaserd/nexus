#pragma once

#include <string_view>

#include "core/Expected.h"

namespace nexus::core {

enum class ParseError {
    Empty,
    NotANumber,
    OutOfRange,
};

// Parses a base-10 positive integer (> 0) that fits into int.
Expected<int, ParseError> parsePositiveInt(std::string_view text);

} // namespace nexus::core
