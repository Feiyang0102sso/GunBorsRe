#pragma once
#include "engine/graphics/CPNG.h"
/**
 * Write an RGBA8 image out as a PNG.
 *
 * Only exists so a milestone harness can screenshot itself and be checked
 * without a human at the keyboard; the game never writes PNGs. Emits the
 * simplest legal file -- colour type 6, no filtering.
 *
 * @param path Destination file, overwritten if present.
 */
bool PNGEncode(const PNGImage &image, const std::string &path);
