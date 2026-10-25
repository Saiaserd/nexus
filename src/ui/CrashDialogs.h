#pragma once

namespace nexus::ui {

// Windows: stops the CRT / OS from opening modal "Debug Error!" or crash dialogs, so an
// automated run (tests, --screenshot, --quit-after-show) fails with a non-zero exit code and a
// message on stderr instead of blocking on a window. No-op elsewhere. Call first thing in main().
void suppressCrashDialogs();

// Calls suppressCrashDialogs() when argv contains an automation flag (--screenshot,
// --quit-after-show, --suppress-crash-dialogs).
void suppressCrashDialogsIfAutomated(int argc, const char* const* argv);

} // namespace nexus::ui
