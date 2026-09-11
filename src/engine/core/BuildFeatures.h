#pragma once

// MSBuild selects the features; both game configurations enable cheats, while Release excludes tests and capture.
#ifndef GB_ENABLE_TESTS
#define GB_ENABLE_TESTS 0
#endif
#ifndef GB_ENABLE_CAPTURE
#define GB_ENABLE_CAPTURE 0
#endif
#ifndef GB_ENABLE_CHEATS
#define GB_ENABLE_CHEATS 0
#endif
#if !GB_ENABLE_CAPTURE
#define GB_SAVE_FRAME(window, path) false
#endif
