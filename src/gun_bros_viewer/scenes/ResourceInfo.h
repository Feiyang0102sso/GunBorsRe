#pragma once
#include "gun_bros_re/data/objects/CGameObjectPack.h"
#include <string>

/** Detect the format from every bound pack; never infer it from directory names. */
bool DetectViewerBigVersion(const std::string &bigDirectory, CGameObjectPack::BigVersion &version, bool detailed = false);
