#pragma once

#include <tl/expected.hpp>

// Project-wide error-handling type (ADR 0002). Use nexus::Expected / nexus::unexpected only;
// never spell tl:: directly so the alias can later be switched to std::expected (C++23).
namespace nexus {

template <class T, class E> using Expected = tl::expected<T, E>;

using tl::unexpected;

} // namespace nexus
