#include "engine/core/ZPaths.h"
/** @file TestOutput.h
 * @brief Host-only output paths for the existing research checks.
 * The driver supplies one absolute directory per case. No game resource values
 * are defined here. Checks are compiled from tests/ only in Debug.
 */
#ifndef GUN_BROS_RE_TEST_OUTPUT_H
#define GUN_BROS_RE_TEST_OUTPUT_H

#include <filesystem>
#include <stdexcept>
#include <string>

namespace TestOutput {
// Only the research launcher configures this before dispatching a check.
// Direct interactive checks use the documented manual output directory.
inline std::filesystem::path directory;
inline std::filesystem::path fixtureDirectory;
inline bool referenceGallery = false;

/** The test driver supplies the fixture directory explicitly; release programs never search the repository. */
inline const std::filesystem::path &Fixtures() {
    if (fixtureDirectory.empty()) { throw std::runtime_error("Missing --fixtures directory"); }
    return fixtureDirectory;
}

inline void Configure(const std::filesystem::path &absoluteDirectory) {
    if (!absoluteDirectory.is_absolute()) {
        throw std::invalid_argument("Test output directory must be absolute");
    }
    directory = absoluteDirectory.lexically_normal();
    std::filesystem::create_directories(directory);
}

/** Resolve an output name without changing the process working directory.
 * Keep legacy relative subdirectories so related fixtures retain their layout.
 * This never deletes previous evidence; only the top-level runner cleans.
 */
inline std::string Path(const std::string &relativeName) {
    if (directory.empty()) {
        Configure(Paths::Root() / "tests/out/manual");
    }
    const auto path = directory / relativeName;
    std::filesystem::create_directories(path.parent_path());
    return path.string();
}
}

#endif
