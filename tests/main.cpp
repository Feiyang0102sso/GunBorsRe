/** Built on demand in Debug; reuses the same command routing and implementations. */
#include "engine/platform/Startup.h"
#include "gun_bros_viewer/ViewerApplication.h"
int wmain(int argc, wchar_t **argv) {
    Utf8Arguments arguments(argc, argv);
    return RunViewerApplication(arguments.Count(), arguments.Data());
}

