#ifndef GUN_BROS_RE_PRODUCT_VERSION_H
#define GUN_BROS_RE_PRODUCT_VERSION_H
/**
 * @brief Version and authorship strings shared by every product's .rc script.
 *
 * Plain macros rather than constants: the resource compiler reads this header,
 * and it understands preprocessor directives only. Bump the version here and
 * both executables follow.
 */

// VERSIONINFO wants the numeric form comma separated and the string form dotted.
#define GB_VERSION_NUMBER 1, 0, 0, 0
#define GB_VERSION_TEXT "1.0.0.0"

#define GB_PRODUCT_NAME "Gun Bros Re"
#define GB_COMPANY_NAME "Feiyang"
#define GB_COPYRIGHT "Copyright (C) 2026 Feiyang"

// Icon resource id. 1 is deliberate: Explorer shows an executable's
// lowest-numbered icon, and SDL's Windows backend loads the same one for its
// window class. Mirrored by ZLaunchDialogConfig::AppIconId.
#define GB_ICON_ID 1

#endif
