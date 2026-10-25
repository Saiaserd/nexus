#include "core/NumberParser.h"

#include <charconv>
#include <memory>
#include <system_error>

namespace nexus::core {

Expected<int, ParseError> parsePositiveInt(std::string_view text)
{
    if (text.empty()) {
        return unexpected(ParseError::Empty);
    }

    int value = 0;
    const char* const first = text.data();
    const char* const last = std::to_address(text.end());
    const auto [ptr, ec] = std::from_chars(first, last, value);

    if (ec == std::errc::result_out_of_range) {
        return unexpected(ParseError::OutOfRange);
    }
    if (ec != std::errc{} || ptr != last) {
        return unexpected(ParseError::NotANumber);
    }
    if (value <= 0) {
        return unexpected(ParseError::OutOfRange);
    }
    return value;
}

} // namespace nexus::core
