/** Viewer entry point; command implementations and resource pages compile separately. */
#include "engine/platform/ZStartup.h"
#include "gun_bros_viewer/ViewerApplication.h"
int wmain(int argc, wchar_t **argv) {
    OpenProductLog(Paths::ViewerName);
    ZUtf8Arguments arguments(argc, argv);
    return RunViewerApplication(arguments.Count(), arguments.Data());
}
