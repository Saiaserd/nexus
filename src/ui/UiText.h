#pragma once

#include <QString>

#include "ui/VaultSession.h"

namespace nexus::ui {

// Text for the title bar: "筆記名 — vault 名 — Nexus"; parts that are empty are left out.
[[nodiscard]] QString formatWindowTitle(const QString& noteName, const QString& vaultName);

// User-facing (Traditional Chinese) explanation of why a vault could not be opened or created.
// The core layer only reports error kinds (ADR 0004); the wording lives here.
[[nodiscard]] QString describeOpenError(const VaultOpenError& error);

} // namespace nexus::ui
