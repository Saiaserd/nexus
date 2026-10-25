#include "ui/CrashDialogs.h"

#include <cstring>
#include <initializer_list>
#include <span>

#ifdef _WIN32
#include <crtdbg.h>
#include <cstdio>
#include <cstdlib>
#include <windows.h>
#endif

namespace nexus::ui {

void suppressCrashDialogs()
{
#ifdef _WIN32
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    // abort(): print the message to stderr, no message box, no Windows Error Reporting.
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    _set_error_mode(_OUT_TO_STDERR);
#ifdef _DEBUG
    for (const int type : {_CRT_WARN, _CRT_ERROR, _CRT_ASSERT}) {
        _CrtSetReportMode(type, _CRTDBG_MODE_FILE);
        _CrtSetReportFile(type, _CRTDBG_FILE_STDERR);
    }
#endif
#endif
}

void suppressCrashDialogsIfAutomated(int argc, const char* const* argv)
{
    if (argc < 1) {
        return;
    }
    for (const char* arg : std::span(argv, static_cast<std::size_t>(argc)).subspan(1)) {
        for (const char* flag : {"--screenshot", "--quit-after-show", "--suppress-crash-dialogs"}) {
            if (std::strcmp(arg, flag) == 0) {
                suppressCrashDialogs();
                return;
            }
        }
    }
}

} // namespace nexus::ui
