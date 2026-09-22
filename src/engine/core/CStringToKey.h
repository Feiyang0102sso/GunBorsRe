/**
 * @file CStringToKey.h
 * @brief The engine-wide string hash.
 *
 * Port of com::glu::platform::core::CStringToKey.
 * Reference: _IDA_OUT/gunbros_3.6.0_IOS.c:370405
 *
 * Every name-based lookup in the engine goes through this function: pack
 * hashes, resource names inside a pack TOC, registry keys.
 * Examples: 
 * 	 pack0_core -> 0x58595522
 *   pack1  .. pack9   -> 0x00267581 .. 0x00267589
 *   pack10 .. pack19  -> 0x01675820 .. 0x01675829 
 
 */

#ifndef GUN_BROS_RE_ENGINE_CSTRINGTOKEY_H
#define GUN_BROS_RE_ENGINE_CSTRINGTOKEY_H

#include <cstdint>

/**
 * Hash a NUL-terminated string.
 *
 * The seed is the string length, then each byte is folded in as
 * `hash = c ^ ROL32(hash, 4)`. Bytes are read as SIGNED chars, so anything
 * >= 0x80 sign-extends before the xor -- do not change this to unsigned.
 *
 * @param text            String to hash. Must not be null.
 * @param caseInsensitive Fold 'A'..'Z' to lower case first. Every call site in
 *                        the game passes false; the true path is included only
 *                        because the original has it.
 */
std::uint32_t CStringToKey(const char *text, bool caseInsensitive = false);

#endif  // GUN_BROS_RE_ENGINE_CSTRINGTOKEY_H
