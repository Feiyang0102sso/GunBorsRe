#include "engine/core/ZPaths.h"
#pragma once
/** @file ZStartupSequenceInternal.h
 * @brief Read original intro video/audio directly; never substitute a still logo.
 */
#define NOMINMAX
#include "gun_bros_re/ZStartupSequence.h"
#include "engine/platform/ZMediaDecoder.h"
#include "engine/platform/ZWindow.h"
#include "engine/graphics/ZQuadBatch.h"
#include "engine/core/ZMatrix4d.h"
#include "engine/platform/ZAudioPlayer.h"
#include "gun_bros_re/gameplay/CBGM.h"
#include <algorithm>
#include <cstdio>
#include <fstream>

namespace StartupSequenceDetail {

const std::filesystem::path kLogoDirectory = Paths::Root() / Paths::StartupDirectory;
}
