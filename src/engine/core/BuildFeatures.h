#pragma once

// MSBuild selects the features; Release excludes test, capture, and cheat implementations.
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

