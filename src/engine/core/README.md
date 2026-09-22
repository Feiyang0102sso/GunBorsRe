# Engine Core

This directory contains small, dependency-light utilities shared by the engine,
resource system, game, viewer, and tests. Code here must not depend on the
resource or gameplay modules.

## Files

| File | Purpose |
| --- | --- |
| `CCrc32.h` | Header-only implementation of the original non-reflected CRC-32 used by profile records. It is intentionally different from zlib CRC-32. |
| `CMatrix4d.h/.cpp` | Declares and implements the row-major 4x4 matrix operations currently required by the 2D renderer and model renderer. |
| `CRandGen.h` | Header-only MT19937 wrapper matching the original `CRandGen` range behavior while allowing explicit desktop seeds. |
| `CStringToKey.h/.cpp` | Declares and implements the original length-seeded, four-bit rotate hash used for pack names, resource names, and other engine keys. |
| `ZPaths.h` | Declares product names, runtime directory conventions, and executable-relative path helpers. The Windows implementation is in `engine/platform/ZPaths.cpp`. |

## Dependency direction

`CStringToKey`, the matrix helpers, CRC, random generation, and path conventions
are general engine facilities. Resource code may depend on this directory, but
this directory must not depend on `engine/resources`.
