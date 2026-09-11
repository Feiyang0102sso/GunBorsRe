#pragma once
#include "gun_bros_re/data/BigVersions.h"
#include <string>

/** Detect the format from every bound pack; never infer it from directory names. */
bool DetectViewerBigVersion(const std::string &bigDirectory, BigVersion &version, bool detailed = false);
