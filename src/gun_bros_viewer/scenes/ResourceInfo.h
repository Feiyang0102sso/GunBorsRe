#pragma once
#include "gun_bros_re/data/ZBigVersions.h"
#include <string>

/** Detect the format from every bound pack; never infer it from directory names. */
bool DetectViewerBigVersion(const std::string &bigDirectory, ZBigVersion &version, bool detailed = false);
