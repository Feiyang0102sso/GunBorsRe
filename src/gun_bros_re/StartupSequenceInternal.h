#include "engine/core/Paths.h"
#pragma once
/** @file StartupSequence.cpp
 * @brief Read original intro video/audio directly; never substitute a still logo.
 */
#define NOMINMAX
#include "gun_bros_re/StartupSequence.h"
#include "engine/platform/CMediaDecoder.h"
#include "engine/platform/CWindow.h"
#include "engine/graphics/CQuadBatch.h"
#include "engine/core/CMatrix4d.h"
#include "engine/platform/CAudioPlayer.h"
#include "gun_bros_re/gameplay/CBGM.h"
#include <algorithm>
#include <cstdio>
#include <fstream>

namespace StartupSequenceDetail {

const std::filesystem::path kLogoDirectory = Paths::Root() / Paths::StartupDirectory;
}
