#pragma once

// MSBuild selects capture per product; both game configurations enable cheats.
// Test sources belong exclusively to the Tests executable.
#ifndef GB_ENABLE_CAPTURE
#define GB_ENABLE_CAPTURE 0
#endif
#ifndef GB_ENABLE_CHEATS
#define GB_ENABLE_CHEATS 0
#endif
#if !GB_ENABLE_CAPTURE
#define GB_SAVE_FRAME(window, path) false
#endif
