/**
 * @file M38Enemy.h
 * @brief M3.8 harness: enemies, assembled by their own scripts.
 */

#ifndef GUN_BROS_RE_MILESTONES_M38ENEMY_H
#define GUN_BROS_RE_MILESTONES_M38ENEMY_H

#include <cstdint>
#include <string>

/**
 * Stand an enemy up out of however many pieces its script asks for.
 *
 * Unlike the player, an enemy's shape is not in its template: the template
 * carries one move set and a script, and the script decides how many parts
 * there are, which move each shows, and which bone each hangs off. So the
 * viewer runs the script and draws whatever comes out.
 *
 * @param startIndex Which enemy of the catalogue to open on; the arrow keys
 *        reach the rest.
 * @param spinDegrees Turn the enemy this far before the first frame.
 * @param screenshotPath When non-empty, save the first frame here and exit.
 * @param advanceMs Run the animation on this far before that first frame.
 * @param bodyMoveIndex Hold this move on the body instead of the one the
 *        script chose, or -1 to leave the script in charge. The same takeover
 *        the M and N keys do, so a screenshot can name an animation.
 */
int RunM38Enemy(const std::string &bigDirectory, std::uint32_t startIndex,
                float spinDegrees, const std::string &screenshotPath,
                std::uint32_t advanceMs, std::int32_t bodyMoveIndex);

/**
 * Run every enemy's script and report the part table it builds. Touches no GL.
 *
 * The number that signs this off is how many enemies assemble to more than one
 * part: if the script hookup were wrong they would all come out as one.
 *
 * @return 0 when every enemy template parsed.
 */
int RunEnemySurvey(const std::string &bigDirectory);

#endif  // GUN_BROS_RE_MILESTONES_M38ENEMY_H
