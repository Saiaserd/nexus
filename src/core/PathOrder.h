#pragma once

#include <QStringView>

namespace nexus::core {

// Natural, case-insensitive ordering: digit runs compare by numeric value, so "note2" sorts
// before "note10". Deterministic and locale independent. Ties are broken case-sensitively so
// the order is total.
bool naturalLess(QStringView a, QStringView b);

// Orders vault-relative paths ('/' separated). Directories come before files at the same
// level, siblings use naturalLess, and a parent comes before its children.
bool pathLess(QStringView a, bool aIsDir, QStringView b, bool bIsDir);

// True when rel is a non-empty relative path that stays inside the root: no leading '/',
// no drive letter, no backslash, no empty / "." / ".." components.
bool isSafeRelPath(QStringView rel);

} // namespace nexus::core
